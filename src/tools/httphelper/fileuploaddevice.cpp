/*
 * fileuploaddevice.cpp
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

#include "fileuploaddevice.h"

FileUploadDevice::FileUploadDevice(QFile* file, const QString& boundaryString)
	: QIODevice()
	, pos_(0)
	, boundaryString_(boundaryString)
{
	setFile(file);
}

FileUploadDevice::~FileUploadDevice()
{
}

QString FileUploadDevice::boundaryString() const
{
	return boundaryString_;
}

void FileUploadDevice::setFile(QFile* file)
{
	Q_ASSERT(file);
	file_ = file;
	file_->setParent(this);
}

QByteArray* FileUploadDevice::preData()
{
	return &preData_;
}

QByteArray* FileUploadDevice::postData()
{
	return &postData_;
}

qint64 FileUploadDevice::size() const
{
	return preData_.size() +
	       file_->size() +
	       postData_.size();
}

qint64 FileUploadDevice::readData(char* data, qint64 maxSize)
{
	qint64 result = 0;
	qint64 len = qMin(size(), maxSize);

	while (len > 0) {
		if (pos_ >= (preData_.size() + file_->size() + postData_.size())) {
			if (result == 0) {
				result = -1;
			}
			break;
		}

		qint64 p = pos_;
		if ((len > 0) && (p >= 0) && pos_ < preData_.size()) {
			qint64 bytesToRead = qMin(len, preData_.size() - p);

			memcpy((data + result), (preData_.constData() + p), bytesToRead);

			pos_ += bytesToRead;
			result += bytesToRead;
			len -= bytesToRead;
		}

		p = pos_ - preData_.size();
		if ((len > 0) && (p >= 0) && pos_ < (preData_.size() + file_->size())) {
			qint64 bytesToRead = qMin(len, file_->size() - p);

			file_->seek(p);
			qint64 readResult = file_->read((data + result), bytesToRead);
			if (readResult != bytesToRead) {
				result = -1;
				break;
			}

			pos_ += bytesToRead;
			result += bytesToRead;
			len -= bytesToRead;
		}

		p = pos_ - preData_.size() - file_->size();
		if ((len > 0) && (p >= 0) && pos_ < (preData_.size() + file_->size() + postData_.size())) {
			qint64 bytesToRead = qMin(len, postData_.size() - p);

			memcpy((data + result), (postData_.constData() + p), bytesToRead);

			pos_ += bytesToRead;
			result += bytesToRead;
			len -= bytesToRead;
		}
	}

	// qWarning("FileUploadDevice::readData(%d) = %d", (int)maxSize, (int)result);
	return result;
}

qint64 FileUploadDevice::writeData(const char* data, qint64 maxSize)
{
	Q_UNUSED(data);
	Q_UNUSED(maxSize);
	return -1;
}
