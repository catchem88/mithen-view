#include "qvocr.h"

#include "quirc/quirc.h"

#include <QtGlobal>
#include <QtMath>

#include <cstring>
#include <vector>

#ifdef WIN32_LOADED
#include <winrt/base.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Storage.Streams.h>
#endif

namespace
{
    //Only lines with more than this many characters are reported
    const int minimumTextLength = 3;

    //Decodes QR codes with quirc and appends them as boxes in the coordinates of the given image
    void decodeQrCodes(const QImage &image,const qreal scale,QList<QVOcrBox> &boxes)
    {
        const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
        if (gray.isNull())
        {
            return;
        }

        struct quirc *qr = quirc_new();
        if (qr == nullptr)
        {
            return;
        }

        if (quirc_resize(qr,gray.width(),gray.height()) < 0)
        {
            quirc_destroy(qr);
            return;
        }

        int width = 0;
        int height = 0;
        uint8_t *buffer = quirc_begin(qr,&width,&height);
        if (buffer != nullptr)
        {
            for (int y = 0; y < height; y++)
            {
                std::memcpy(buffer + size_t(y) * size_t(width),gray.constScanLine(y),size_t(width));
            }
        }
        quirc_end(qr);

        const int count = quirc_count(qr);
        for (int i = 0; i < count; i++)
        {
            struct quirc_code code;
            struct quirc_data data;
            quirc_extract(qr,i,&code);
            if (quirc_decode(&code,&data) != QUIRC_SUCCESS)
            {
                continue;
            }

            //The box is the bounding rectangle of the code's four corners
            int left = code.corners[0].x;
            int top = code.corners[0].y;
            int right = left;
            int bottom = top;
            for (int corner = 1; corner < 4; corner++)
            {
                left = qMin(left,code.corners[corner].x);
                top = qMin(top,code.corners[corner].y);
                right = qMax(right,code.corners[corner].x);
                bottom = qMax(bottom,code.corners[corner].y);
            }

            const QRect rect(qRound(left / scale),qRound(top / scale),
                qRound((right - left) / scale),qRound((bottom - top) / scale));
            if (rect.isEmpty())
            {
                continue;
            }

            boxes.append({rect,QString::fromUtf8(reinterpret_cast<const char *>(data.payload),data.payload_len)});
        }

        quirc_destroy(qr);
    }

#ifdef WIN32_LOADED
    winrt::Windows::Media::Ocr::OcrEngine createOcrEngine()
    {
        //Prefer a language from the user profile, then any installed OCR language
        const winrt::Windows::Media::Ocr::OcrEngine profileEngine =
            winrt::Windows::Media::Ocr::OcrEngine::TryCreateFromUserProfileLanguages();
        if (profileEngine != nullptr)
        {
            return profileEngine;
        }

        for (const winrt::Windows::Globalization::Language &language :
             winrt::Windows::Media::Ocr::OcrEngine::AvailableRecognizerLanguages())
        {
            const winrt::Windows::Media::Ocr::OcrEngine languageEngine =
                winrt::Windows::Media::Ocr::OcrEngine::TryCreateFromLanguage(language);
            if (languageEngine != nullptr)
            {
                return languageEngine;
            }
        }

        return nullptr;
    }

    qreal getOcrScale(const QSize &imageSize)
    {
        const qreal maxDimension = qreal(winrt::Windows::Media::Ocr::OcrEngine::MaxImageDimension());
        const int longestSide = qMax(imageSize.width(),imageSize.height());
        if (longestSide <= 0 || qreal(longestSide) <= maxDimension)
        {
            return 1.0;
        }

        return maxDimension / qreal(longestSide);
    }
#endif
}

bool QVOcr::isAvailable()
{
#ifdef WIN32_LOADED
    try
    {
        return createOcrEngine() != nullptr;
    }
    catch (...)
    {
        return false;
    }
#else
    return false;
#endif
}

QVOcrResult QVOcr::recognize(const QImage &image)
{
    QVOcrResult result;

    if (image.isNull())
    {
        result.errorMessage = QStringLiteral("empty image");
        return result;
    }

#ifdef WIN32_LOADED
    try
    {
        //The OCR engine reads BGRA8 pixels only
        const QImage source = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        const qreal scale = getOcrScale(source.size());
        const QImage prepared = qFuzzyCompare(scale,1.0)
            ? source
            : source.scaled(qRound(source.width() * scale),qRound(source.height() * scale),
                Qt::IgnoreAspectRatio,Qt::SmoothTransformation);

        //Text recognition needs an installed OCR language, QR codes do not
        const winrt::Windows::Media::Ocr::OcrEngine engine = createOcrEngine();
        if (engine != nullptr)
        {
            const int width = prepared.width();
            const int height = prepared.height();
            const int stride = width * 4;

            std::vector<uint8_t> pixels(size_t(stride) * size_t(height));
            for (int y = 0; y < height; y++)
            {
                std::memcpy(pixels.data() + size_t(y) * size_t(stride),prepared.constScanLine(y),size_t(stride));
            }

            const winrt::Windows::Storage::Streams::IBuffer buffer =
                winrt::Windows::Security::Cryptography::CryptographicBuffer::CreateFromByteArray(
                    winrt::array_view<const uint8_t>(pixels.data(),pixels.data() + pixels.size()));
            const winrt::Windows::Graphics::Imaging::SoftwareBitmap bitmap =
                winrt::Windows::Graphics::Imaging::SoftwareBitmap::CreateCopyFromBuffer(buffer,
                    winrt::Windows::Graphics::Imaging::BitmapPixelFormat::Bgra8,width,height,
                    winrt::Windows::Graphics::Imaging::BitmapAlphaMode::Premultiplied);

            try
            {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);
            }
            catch (...)
            {
                //The thread already has an apartment, which is fine
            }

            const winrt::Windows::Media::Ocr::OcrResult ocrResult = engine.RecognizeAsync(bitmap).get();
            for (const winrt::Windows::Media::Ocr::OcrLine &line : ocrResult.Lines())
            {
                const QString text = QString::fromWCharArray(line.Text().c_str()).trimmed();
                if (text.length() <= minimumTextLength)
                {
                    continue;
                }

                //WinRT reports words only, so a line's box is the union of its words
                QRect lineRect;
                for (const winrt::Windows::Media::Ocr::OcrWord &word : line.Words())
                {
                    const winrt::Windows::Foundation::Rect wordRect = word.BoundingRect();
                    const QRect mappedRect(qRound(wordRect.X / scale),qRound(wordRect.Y / scale),
                        qRound(wordRect.Width / scale),qRound(wordRect.Height / scale));
                    lineRect = lineRect.isNull() ? mappedRect : lineRect.united(mappedRect);
                }

                if (lineRect.isNull())
                {
                    continue;
                }

                result.boxes.append({lineRect,text});
            }
        }

        decodeQrCodes(prepared,scale,result.boxes);

        result.isSuccessful = true;
    }
    catch (const winrt::hresult_error &error)
    {
        result.errorMessage = QString::fromWCharArray(error.message().c_str());
    }
    catch (...)
    {
        result.errorMessage = QStringLiteral("unknown error");
    }
#else
    result.errorMessage = QStringLiteral("unsupported platform");
#endif

    return result;
}
