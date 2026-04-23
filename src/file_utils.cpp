#include "file_utils.h"

#include <QUrl>
#include <QDesktopServices>

namespace file_utils
{
	void openFileInNotepad(const QString& filePath)
	{
		QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
	}
};