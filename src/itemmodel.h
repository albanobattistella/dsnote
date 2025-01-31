/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ITEMMODEL_H
#define ITEMMODEL_H

#include <QList>
#include <QObject>
#include <QThread>
#include <memory>

#include "listmodel.h"

class ItemModel;

class ItemWorker : public QThread {
    Q_OBJECT
    friend class ItemModel;
    friend class SelectableItemModel;

   public:
    explicit ItemWorker(ItemModel *model, const QString &data = {});
    ~ItemWorker();

   private:
    QString data;
    ItemModel *model;
    QList<ListItem *> items;
    void run() final;
};

class ItemModel : public ListModel {
    Q_OBJECT
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(int count READ getCount NOTIFY countChanged)

    friend class ItemWorker;

   public:
    explicit ItemModel(ListItem *prototype, QObject *parent = nullptr);
    ~ItemModel();
    int getCount() const;
    bool isBusy() const;

   signals:
    void busyChanged();
    void countChanged();

   public slots:
    virtual void updateModel(const QString &data = {});

   protected:
    std::unique_ptr<ItemWorker> m_worker;
    virtual QList<ListItem *> makeItems() = 0;
    virtual void postMakeItems(const QList<ListItem *> &items);
    virtual void clear();
    virtual size_t firstChangedItemIdx(const QList<ListItem *> &oldItems,
                                       const QList<ListItem *> &newItems);
    void setBusy(bool busy);

   protected slots:
    virtual void workerDone();

   private:
    bool m_busy = true;
};

class SelectableItem : public ListItem {
    Q_OBJECT

   public:
    explicit SelectableItem(QObject *parent = nullptr) : ListItem{parent} {}
    inline bool selected() const { return m_selected; }
    inline bool selectable() const { return m_selectable; }
    void setSelected(bool value);

   protected:
    bool m_selectable = true;

   private:
    bool m_selected = false;
};

class SelectableItemModel : public ItemModel {
    Q_OBJECT
    Q_PROPERTY(
        QString filter READ getFilter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectedCountChanged)
    Q_PROPERTY(
        int selectableCount READ selectableCount NOTIFY selectableCountChanged)

   public:
    explicit SelectableItemModel(SelectableItem *prototype,
                                 QObject *parent = nullptr);
    void setFilter(const QString &filter);
    void setFilterNoUpdate(const QString &filter);
    const QString &getFilter() const;
    int selectedCount() const;
    int selectableCount() const;

    Q_INVOKABLE void setSelected(int index, bool value);
    Q_INVOKABLE void setAllSelected(bool value);
    Q_INVOKABLE virtual QVariantList selectedItems();

   public slots:
    void updateModel(const QString &data = {}) override;

   signals:
    void filterChanged();
    void selectedCountChanged();
    void selectableCountChanged();

   protected slots:
    void workerDone() override;

   private:
    QString m_filter;
    int m_selectedCount = 0;
    int m_selectableCount = 0;
    void clear() override;
    void postMakeItems(const QList<ListItem *> &items) override;
};

#endif  // ITEMMODEL_H
