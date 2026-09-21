#ifndef QVOCR_H
#define QVOCR_H

#include <QImage>
#include <QList>
#include <QRect>
#include <QString>

struct QVOcrBox
{
    QRect rect;
    QString text;
};

struct QVOcrResult
{
    bool isSuccessful = false;
    QString errorMessage;
    QList<QVOcrBox> boxes;
};

namespace QVOcr
{
    //Whether an OCR engine with at least one language is available
    bool isAvailable();

    //Recognizes text in the given image. Blocking, so call it from a worker thread.
    QVOcrResult recognize(const QImage &image);
}

#endif // QVOCR_H
