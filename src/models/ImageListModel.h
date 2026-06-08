#pragma once

#include "database/ImageRepository.h"
#include "services/ThumbnailCache.h"

#include <QAbstractTableModel>

class ImageListModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { ThumbnailColumn, FileNameColumn, RelativePathColumn, SizeColumn, FileSizeColumn, MatchCountColumn, StatusColumn, ColumnCount };

    explicit ImageListModel(ImageRepository* repository, ThumbnailCache* thumbnails, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void reload(const ImageFilter& filter = {});
    ImageRecord imageAt(int row) const;
    int totalImageCount() const;
    int statusCount(ImageStatus status) const;

signals:
    void imageActivated(const ImageRecord& image);

private:
    ImageRepository* m_repository = nullptr;
    ThumbnailCache* m_thumbnails = nullptr;
    QVector<ImageRecord> m_images;
    ImageFilter m_filter;
};
