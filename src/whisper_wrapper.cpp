/* Copyright (C) 2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "whisper_wrapper.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>

#include "cpu_tools.hpp"
#include "logger.hpp"

whisper_wrapper::whisper_wrapper(config_t config, callbacks_t call_backs)
    : engine_wrapper{std::move(config), std::move(call_backs)},
      m_wparams{make_wparams()} {
    m_speech_buf.reserve(m_speech_max_size);
    start_engine();
}

whisper_wrapper::~whisper_wrapper() {
    LOGD("whisper dtor");

    stop_processing();
}

void whisper_wrapper::push_buf_to_whisper_buf(
    const std::vector<in_buf_t::buf_t::value_type>& buf,
    whisper_buf_t& whisper_buf) {
    std::transform(buf.cbegin(), buf.cend(), std::back_inserter(whisper_buf),
                   [](auto sample) {
                       return static_cast<whisper_buf_t::value_type>(sample) /
                              32768.0F;
                   });
}

bool whisper_wrapper::sentence_timer_timed_out() {
    if (m_start_time) {
        if (m_speech_buf.empty() &&
            m_timeout > std::chrono::steady_clock::now() - *m_start_time) {
            LOGD("sentence timeout");
            return true;
        }
    } else {
        LOGD("staring sentence timer");
        m_start_time = std::chrono::steady_clock::now();
    }

    return false;
}

void whisper_wrapper::reset_impl() { m_speech_buf.clear(); }

void whisper_wrapper::stop_processing_impl() {
    if (m_whisper_ctx) {
        LOGD("whisper cancel");
        whisper_cancel(m_whisper_ctx.get());
    }
}

void whisper_wrapper::init_whisper() {
    if (m_whisper_ctx) return;

    LOGD("initing whisper");

    m_whisper_ctx =
        whisper_ctx_t{whisper_init_from_file(m_model_file.first.c_str()),
                      [](whisper_context* ctx) { whisper_free(ctx); }};

    LOGD("whisper inited successfully");
}

engine_wrapper::samples_process_result_t whisper_wrapper::process_buff() {
    init_whisper();

    if (!lock_buff_for_processing())
        return samples_process_result_t::wait_for_samples;

    auto eof = m_in_buf.eof;
    auto sof = m_in_buf.sof;

    LOGD("process samples buf: mode="
         << m_speech_mode << ", in-buf size=" << m_in_buf.size
         << ", speech-buf size=" << m_speech_buf.size() << ", sof=" << sof
         << ", eof=" << eof);

    if (sof) {
        m_speech_buf.clear();
        m_start_time.reset();
        m_vad.reset();
    }

    const auto& vad_buf =
        m_vad.process(m_in_buf.buf.data(), m_in_buf.buf.size());

    m_in_buf.clear();

    bool vad_status = !vad_buf.empty();

    if (vad_status) {
        LOGD("vad: speech detected");

        if (m_speech_mode != speech_mode_t::manual)
            set_speech_detection_status(
                speech_detection_status_t::speech_detected);

        push_buf_to_whisper_buf(vad_buf, m_speech_buf);
    } else {
        LOGD("vad: no speech");

        if (m_speech_mode == speech_mode_t::single_sentence &&
            sentence_timer_timed_out()) {
            m_call_backs.sentence_timeout();
        }

        if (m_speech_mode == speech_mode_t::automatic)
            set_speech_detection_status(speech_detection_status_t::no_speech);
    }

    auto decode_samples = [&] {
        if (m_speech_buf.size() > m_speech_max_size) {
            LOGD("speech buf reached max size");
            return true;
        }

        if (m_speech_buf.empty()) return false;

        if ((m_speech_mode != speech_mode_t::manual ||
             m_speech_detection_status ==
                 speech_detection_status_t::speech_detected) &&
            vad_status && !eof)
            return false;

        if (m_speech_mode == speech_mode_t::manual &&
            m_speech_detection_status == speech_detection_status_t::no_speech &&
            !eof)
            return false;

        return true;
    }();

    if (!decode_samples) {
        if (m_speech_mode == speech_mode_t::manual &&
            m_speech_detection_status == speech_detection_status_t::no_speech) {
            flush(eof ? flush_t::eof : flush_t::regular);
            free_buf();
            return samples_process_result_t::no_samples_needed;
        }

        free_buf();
        return samples_process_result_t::wait_for_samples;
    }

    if (m_thread_exit_requested) {
        free_buf();
        return samples_process_result_t::no_samples_needed;
    }

    auto old_status = m_speech_detection_status;

    set_speech_detection_status(speech_detection_status_t::decoding);

    LOGD("speech frame: samples=" << m_speech_buf.size());

    decode_speech(m_speech_buf);

    m_speech_buf.clear();

    set_speech_detection_status(old_status);

    flush(eof ? flush_t::eof : flush_t::regular);

    free_buf();

    return samples_process_result_t::wait_for_samples;
}

whisper_full_params whisper_wrapper::make_wparams() const {
    whisper_full_params wparams =
        whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.language = m_lang.c_str();
    wparams.speed_up = true;
    wparams.temperature_inc = 0.0F;
    wparams.suppress_blank = true;
    wparams.suppress_non_speech_tokens = true;
    wparams.single_segment = false;
    wparams.translate = m_translate;
    wparams.no_context = true;
    wparams.n_threads = std::min(2, cpu_tools::number_of_cores());

    LOGD("cpu info: arch=" << cpu_tools::arch()
                           << ", cores=" << cpu_tools::number_of_cores()
                           << ", neon=" << cpu_tools::neon_supported());
    LOGD("using threads: " << wparams.n_threads << "/"
                           << cpu_tools::number_of_cores());
    LOGD("system info: " << whisper_print_system_info());

    return wparams;
}

void whisper_wrapper::decode_speech(const whisper_buf_t& buf) {
    std::ostringstream os;

    LOGD("speech decoding started");

    auto decoding_start = std::chrono::steady_clock::now();

    if (whisper_full(m_whisper_ctx.get(), m_wparams, buf.data(), buf.size()) ==
        0) {
        auto n = whisper_full_n_segments(m_whisper_ctx.get());
        LOGD("wisper segments: " << n);

        for (auto i = 0; i < n; ++i) {
            std::string text =
                whisper_full_get_segment_text(m_whisper_ctx.get(), i);
            rtrim(text);
            ltrim(text);

            LOGT("whisper segment: " << text);

            if (i != 0) os << ' ';

            os << text;
        }
    } else {
        LOGE("whisper error");
        return;
    }

    auto decoding_dur = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - decoding_start)
                            .count();

    LOGD("decoding stats: samples="
         << buf.size() << ", duration=" << decoding_dur << "ms ("
         << static_cast<double>(decoding_dur) /
                ((1000 * buf.size()) / static_cast<double>(m_sample_rate))
         << ")");

    auto result =
        merge_texts(m_intermediate_text.value_or(std::string{}), os.str());

    if (!m_intermediate_text || m_intermediate_text != result)
        set_intermediate_text(result);
}
