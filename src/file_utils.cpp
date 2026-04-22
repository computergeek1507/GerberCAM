#include "file_utils.h"

#include <QUrl>
#include <QDesktopServices>

namespace FileUtils
{
	void openFileInNotepad(const std::filesystem::path& filePath)
	{
		QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(filePath.string())));


	}
};