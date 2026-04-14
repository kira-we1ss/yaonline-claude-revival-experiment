/*
 * fileuploaddevice.h
 * Copyright (C) 2010  Yandex LLC (Michail Pishchagin)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef FILEUPLOADDEVICE_H
#define FILEUPLOADDEVICE_H

#include <QIODevice>
#include <QPointer>
#include <QFile>

class FileUploadDevice : public QIODevice
{
	Q_OBJECT
public:
	FileUploadDevice(QFile* file, const QString& boundaryString);
	~FileUploadDevice();

	void setFile(QFile* file);
	QByteArray* preData();
	QByteArray* postData();

	QString boundaryString() const;

	// reimplemented
	qint64 size() const;

protected:
	// reimplemented
	virtual qint64 readData(char* data, qint64 maxSize);
        virtual qint64 writeData(const char* data, qint64 maxSize);

private:
	QPointer<QFile> file_;
	QByteArray preData_;
	QByteArray postData_;
	qint64 pos_;
	QString boundaryString_;
};

#endif
