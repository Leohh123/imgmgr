#include "models/ImageListModel.h"

#include "utils/ImageTableUtils.h"

#include <QBrush>
#include <QFontMetrics>
#include <QIcon>
#include <QApplication>
#include <QPixmap>
#include <QSize>

namespace {
constexpr int ThumbnailExtent = 128;

QVariant displayValueForColumn(const ImageRecord& image, int column)
{
    switch (column) {
    case ImageListModel::FileNameColumn: return image.fileName;
    case ImageListModel::RelativePathColumn: return image.relativePath;
    case ImageListModel::SizeColumn: return QStringLiteral("%1 x %2").arg(image.width).arg(image.height);
    case ImageListModel::FileSizeColumn: return QString::number(image.fileSize / 1024.0, 'f', 1) + QStringLiteral(" KB");
    case ImageListModel::MatchCountColumn: return image.matchCount;
    case ImageListModel::StatusColumn: return imageStatusText(image.status);
    default: return {};
    }
}

QSize rowSizeHint(const ImageRecord& image, int column)
{
    const int textHeight = QFontMetrics(QApplication::font()).height() + 10;
    int imageHeight = 0;
    if (image.width > 0 && image.height > 0) {
        QSize shown(image.width, image.height);
        shown.scale(QSize(ThumbnailExtent, ThumbnailExtent), Qt::KeepAspectRatio);
        imageHeight = shown.height() + 8;
    }
    const int rowHeight = qMax(textHeight, imageHeight);
    return column == ImageListModel::ThumbnailColumn
        ? QSize(ThumbnailExtent, rowHeight)
        : QSize(80, rowHeight);
}
}

ImageListModel::ImageListModel(ImageRepository* repository, ThumbnailCache* thumbnails, QObject* parent)
    : QAbstractTableModel(parent)
    , m_repository(repository)
    , m_thumbnails(thumbnails)
{
    if (m_thumbnails) {
        connect(m_thumbnails, &ThumbnailCache::thumbnailReady, this, [this](int imageId) {
            for (int row = 0; row < m_images.size(); ++row) {
                if (m_images[row].id == imageId) {
                    emit dataChanged(index(row, ThumbnailColumn), index(row, ThumbnailColumn), { Qt::DecorationRole });
                    break;
                }
            }
        });
    }
}

int ImageListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_images.size();
}

int ImageListModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ImageListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_images.size())
        return {};
    const auto& image = m_images.at(index.row());
    if (role == Qt::DecorationRole && index.column() == ThumbnailColumn && m_thumbnails)
        return QPixmap::fromImage(m_thumbnails->thumbnail(image, QSize(ThumbnailExtent, ThumbnailExtent)))
            .scaled(ThumbnailExtent, ThumbnailExtent, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (role == Qt::SizeHintRole)
        return rowSizeHint(image, index.column());
    if (role == Qt::DisplayRole)
        return displayValueForColumn(image, index.column());
    if (role == Qt::BackgroundRole && image.status == ImageStatus::Conflict)
        return QBrush(QColor(255, 220, 220));
    if (role == Qt::UserRole)
        return image.id;
    return {};
}

QVariant ImageListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    return ImageTableUtils::columnTitle(section);
}

Qt::ItemFlags ImageListModel::flags(const QModelIndex& index) const
{
    return QAbstractTableModel::flags(index) | (index.isValid() ? Qt::ItemIsSelectable : Qt::NoItemFlags);
}

void ImageListModel::reload(const ImageFilter& filter)
{
    if (!m_repository)
        return;
    beginResetModel();
    m_filter = filter;
    m_images = m_repository->fetchImages(filter);
    endResetModel();
}

ImageRecord ImageListModel::imageAt(int row) const
{
    if (row < 0 || row >= m_images.size())
        return {};
    return m_images.at(row);
}

int ImageListModel::totalImageCount() const
{
    return m_images.size();
}

int ImageListModel::statusCount(ImageStatus status) const
{
    int count = 0;
    for (const ImageRecord& image : m_images) {
        if (image.status == status)
            ++count;
    }
    return count;
}
