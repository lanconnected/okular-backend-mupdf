/***************************************************************************
 *   Copyright (C) 2008 by Pino Toscano <pino@kde.org>                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "page.hpp"

extern "C" {
#include <mupdf/fitz.h>
}

#include <QDebug>
#include <QImage>
#include <QSharedData>

namespace QMuPDF
{

QRectF convert_fz_rect(const fz_rect &rect, const QSizeF &dpi)
{
    const float scaleX = dpi.width() / 72.;
    const float scaleY = dpi.height() / 72.;
    const QPointF topLeft(rect.x0 * scaleX, rect.y0 * scaleY);
    const QPointF bottomRight(rect.x1 * scaleX, rect.y1 * scaleY);
    return QRectF(topLeft, bottomRight);
}

QImage convert_fz_pixmap(fz_context *ctx, fz_pixmap *image)
{
    const int w = fz_pixmap_width(ctx, image);
    const int h = fz_pixmap_height(ctx, image);
    QImage img(w, h, QImage::Format_RGBA8888);

    if (img.bytesPerLine() == fz_pixmap_stride(ctx, image)) {
        memcpy(img.bits(), fz_pixmap_samples(ctx, image), img.sizeInBytes());
    } else {
        qWarning() << "QImage line stride" << img.bytesPerLine() << "doesn't match" << fz_pixmap_stride(ctx, image);
        QRgb *data = reinterpret_cast<QRgb *>(fz_pixmap_samples(ctx, image));

        for (int i = 0; i < h; ++i) {
            QRgb *imgdata = reinterpret_cast<QRgb *>(img.scanLine(h));

            for (int j = 0; j < w; ++j) {
                *imgdata = *data;
                data++;
                imgdata++;
            }
        }
    }

    return img;
}

struct Page::Data : public QSharedData {
    Data(int pageNum, fz_context *ctx, fz_document *doc, fz_page *page) : pageNum{pageNum}, ctx{ctx}, doc{doc}, page{page} {}
    int pageNum;
    fz_context *ctx;
    fz_document *doc;
    fz_page *page;
};

Page::~Page()
{
    fz_drop_page(d->ctx, d->page);
}

Page::Page(fz_context *ctx, fz_document *doc, int num) :
    d(new Page::Data(num, ctx, doc, fz_load_page(ctx, doc, num)))
{
    Q_ASSERT(doc && ctx);
}

Page::Page(const Page &other) = default;

int Page::number() const
{
    return d->pageNum;
}

QSizeF Page::size(const QSizeF &dpi) const
{
    fz_rect rect = fz_bound_page(d->ctx, d->page);
    // MuPDF always assumes 72dpi
    return QSizeF((rect.x1 - rect.x0) * dpi.width() / 72.,
                  (rect.y1 - rect.y0) * dpi.height() / 72.);
}

qreal Page::duration() const
{
    float val;
    (void)fz_page_presentation(d->ctx, d->page, nullptr, &val);
    return val < 0.1 ? -1 : val;
}

QImage Page::render(qreal width, qreal height) const
{
    const QSizeF s = size(QSizeF(72, 72));
    fz_matrix ctm = fz_scale(width / s.width(), height / s.height());
    fz_cookie cookie = { 0, 0, 0, 0, 0 };
    fz_colorspace *csp = fz_device_rgb(d->ctx);
    fz_pixmap *image = fz_new_pixmap(d->ctx, csp, width, height, nullptr, 1);
    fz_clear_pixmap_with_value(d->ctx, image, 0xff);
    fz_device *device = fz_new_draw_device(d->ctx, fz_identity, image);
    fz_run_page(d->ctx, d->page, device, ctm, &cookie);
    fz_close_device(d->ctx, device);
    fz_drop_device(d->ctx, device);
    QImage img;

    if (!cookie.errors) {
        img = convert_fz_pixmap(d->ctx, image);
    }

    fz_drop_pixmap(d->ctx, image);
    return img;
}

QVector<TextBox *> Page::textBoxes(const QSizeF &dpi) const
{
    fz_cookie cookie = {0, 0, 0, 0, 0};
    fz_stext_page *page = fz_new_stext_page(d->ctx, fz_empty_rect);
    fz_stext_options options{};
    fz_device *device = fz_new_stext_device(d->ctx, page, &options);
    fz_run_page(d->ctx, d->page, device, fz_identity, &cookie);
    fz_close_device(d->ctx, device);
    fz_drop_device(d->ctx, device);

    if (cookie.errors) {
        fz_drop_stext_page(d->ctx, page);
        return QVector<TextBox *>();
    }

    QVector<TextBox *> boxes;

    for (fz_stext_block *block = page->first_block; block; block = block->next) {
        if (block->type != FZ_STEXT_BLOCK_TEXT) {
            continue;
        }

        for (fz_stext_line *line = block->u.t.first_line; line; line = line->next) {
            bool hasText = false;

            for (fz_stext_char *ch = line->first_char; ch; ch = ch->next) {
                const int text = ch->c;
                TextBox *box = new TextBox(text, convert_fz_rect(fz_rect_from_quad(ch->quad), dpi));
                boxes.append(box);
                hasText = true;
            }

            if (hasText) {
                boxes.back()->markAtEndOfLine();
            }
        }
    }

    fz_drop_stext_page(d->ctx, page);
    return boxes;
}

} // namespace QMuPDF
