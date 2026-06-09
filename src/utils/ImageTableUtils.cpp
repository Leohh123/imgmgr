#include "utils/ImageTableUtils.h"

#include "models/ImageListModel.h"

#include <QHeaderView>
#include <QTableView>

namespace ImageTableUtils {

QString columnTitle(int column)
{
    switch (column) {
    case ImageListModel::ThumbnailColumn: return QStringLiteral("缩略图");
    case ImageListModel::FileNameColumn: return QStringLiteral("文件名");
    case ImageListModel::RelativePathColumn: return QStringLiteral("相对路径");
    case ImageListModel::SizeColumn: return QStringLiteral("图片尺寸");
    case ImageListModel::FileSizeColumn: return QStringLiteral("文件大小");
    case ImageListModel::MatchCountColumn: return QStringLiteral("命中规则数量");
    case ImageListModel::StatusColumn: return QStringLiteral("状态");
    default: return {};
    }
}

bool isDefaultColumnVisible(int column)
{
    return column == ImageListModel::ThumbnailColumn
        || column == ImageListModel::FileNameColumn
        || column == ImageListModel::SizeColumn
        || column == ImageListModel::StatusColumn;
}

void configureColumns(QTableView* table)
{
    if (!table)
        return;

    QHeaderView* header = table->horizontalHeader();
    header->setStretchLastSection(false);
    header->setMinimumSectionSize(24);

    header->setSectionResizeMode(ImageListModel::ThumbnailColumn, QHeaderView::Fixed);
    table->setColumnWidth(ImageListModel::ThumbnailColumn, 128);

    header->setSectionResizeMode(ImageListModel::FileNameColumn, QHeaderView::Stretch);

    header->setSectionResizeMode(ImageListModel::RelativePathColumn, QHeaderView::Interactive);
    table->setColumnWidth(ImageListModel::RelativePathColumn, 220);

    header->setSectionResizeMode(ImageListModel::SizeColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ImageListModel::FileSizeColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ImageListModel::MatchCountColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ImageListModel::StatusColumn, QHeaderView::ResizeToContents);
}

}
