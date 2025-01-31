/* Copyright (C) 2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef DENOISER_HPP
#define DENOISER_HPP

#include <array>
#include <cstddef>
#include <cstdint>

struct DenoiseState;

class denoiser {
   public:
    using sample_t = int16_t;

    denoiser();
    ~denoiser();
    void process(sample_t* buf, size_t size);

   private:
    using frame_t = std::array<float, 480>;

    inline static const size_t max_frame_size = 72000;
    inline static const int sample_rate = 16000;

    DenoiseState* m_state = nullptr;

    void normalize_audio(sample_t* audio, size_t size);
};

#endif // DENOISER_HPP
