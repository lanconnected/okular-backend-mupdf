/***************************************************************************
 *   Copyright (C) 2008 by Pino Toscano <pino@kde.org>                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "document.hpp"
#include "page.hpp"

extern "C" {
#include <mupdf/pdf.h>
}

#include <QFile>

#include <cstring>

namespace QMuPDF
{

QRectF convert_fz_rect(const fz_rect &rect, const QSizeF &dpi);

struct Document::Data {
    Data()
        : ctx(fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT))
        , mdoc(nullptr), stream(nullptr), pageCount(0), info(nullptr)
        , pageMode(Document::UseNone), locked(false) { }

    fz_context *ctx;
    fz_document *mdoc;
    fz_stream *stream;
    int pageCount;
    pdf_obj *info;
    PageMode pageMode;
    bool locked;

    pdf_document *pdf() const
    {
        return reinterpret_cast<pdf_document *>(mdoc);
    }
    pdf_obj *dict(const char *key) const
    {
        return pdf_dict_gets(ctx, pdf_trailer(ctx, pdf()), key);
    }
    void loadInfoDict()
    {
        if (!info) {
            info = dict("Info");
        }
    }
    bool load()
    {
        pdf_obj *root = dict("Root");

        if (!root) {
            return false;
        }

        pageCount = fz_count_pages(ctx, mdoc);
        pdf_obj *obj = pdf_dict_gets(ctx, root, "PageMode");

        if (obj && pdf_is_name(ctx, obj)) {
            const char *mode = pdf_to_name(ctx, obj);

            if (!std::strcmp(mode, "UseNone")) {
                pageMode = Document::UseNone;
            } else if (!std::strcmp(mode, "UseOutlines")) {
                pageMode = Document::UseOutlines;
            } else if (!std::strcmp(mode, "UseThumbs")) {
                pageMode = Document::UseThumbs;
            } else if (!std::strcmp(mode, "FullScreen")) {
                pageMode = Document::FullScreen;
            } else if (!std::strcmp(mode, "UseOC")) {
                pageMode = Document::UseOC;
            } else if (!std::strcmp(mode, "UseAttachments")) {
                pageMode = Document::UseAttachments;
            }
        }

        return true;
    }
    void convertOutline(fz_outline *out, Outline *item)
    {
        for (; out; out = out->next) {
            Outline *child = new Outline(out);
            item->appendChild(child);
            convertOutline(out->down, child);
        }
    }
};

Document::Document()
    : d(new Data)
{
    fz_register_document_handlers(d->ctx);
}

Document::~Document()
{
    close();
    fz_drop_context(d->ctx);
    delete d;
}

bool Document::load(const QString &fileName)
{
    QByteArray fileData = QFile::encodeName(fileName);
    d->stream = fz_open_file(d->ctx, fileData.constData());

    if (!d->stream) {
        return false;
    }

    char *oldlocale = std::setlocale(LC_NUMERIC, "C");
    fz_try(d->ctx) {
        d->mdoc = fz_open_document_with_stream(d->ctx, "pdf", d->stream);
    }
    fz_catch(d->ctx) {
        qWarning() << "Error when trying to load document";
        d->mdoc = nullptr;
        return false;
    }

    if (oldlocale) {
        std::setlocale(LC_NUMERIC, oldlocale);
    }

    if (!d->mdoc) {
        return false;
    }

    d->locked = fz_needs_password(d->ctx, d->mdoc);

    if (!d->locked) {
        if (!d->load()) {
            return false;
        }
    }

    return true;
}

void Document::close()
{
    if (!d->mdoc) {
        return;
    }

    fz_drop_document(d->ctx, d->mdoc);
    d->mdoc = nullptr;
    fz_drop_stream(d->ctx, d->stream);
    d->stream = nullptr;
    d->pageCount = 0;
    d->info = nullptr;
    d->pageMode = UseNone;
    d->locked = false;
}

bool Document::isLocked() const
{
    return d->locked;
}

bool Document::unlock(const QByteArray &password)
{
    if (!d->locked) {
        return false;
    }

    QByteArray a = password;

    if (!fz_authenticate_password(d->ctx, d->mdoc, a.data())) {
        return false;
    }

    d->locked = false;

    if (!d->load()) {
        return false;
    }

    return true;
}

int Document::pageCount() const
{
    return d->pageCount;
}

Page Document::page(int pageno) const
{
    return Page(d->ctx, d->mdoc, pageno);
}

QList<QByteArray> Document::infoKeys() const
{
    QList<QByteArray> keys;

    if (!d->mdoc) {
        return keys;
    }

    d->loadInfoDict();

    if (!d->info) {
        return keys;
    }

    const int dictSize = pdf_dict_len(d->ctx, d->info);

    for (int i = 0; i < dictSize; ++i) {
        pdf_obj *obj = pdf_dict_get_key(d->ctx, d->info, i);

        if (pdf_is_name(d->ctx, obj)) {
            keys.append(QByteArray(pdf_to_name(d->ctx, obj)));
        }
    }

    return keys;
}

QString Document::infoKey(const QByteArray &key) const
{
    if (!d->mdoc) {
        return QString();
    }

    d->loadInfoDict();

    if (!d->info) {
        return QString();
    }

    pdf_obj *obj = pdf_dict_gets(d->ctx, d->info, key.constData());

    if (obj) {
        obj = pdf_resolve_indirect(d->ctx, obj);

        if (!pdf_is_string(d->ctx, obj)) {
            qWarning() << "info object not a string!";
            return {};
        }

        char *value = pdf_new_utf8_from_pdf_string_obj(d->ctx, obj);

        if (value) {
            const QString res = QString::fromUtf8(value);
            fz_free(d->ctx, value);
            return res;
        }
    }

    return QString();
}

Outline *Document::outline() const
{
    fz_outline *out = fz_load_outline(d->ctx, d->mdoc);

    if (!out) {
        return nullptr;
    }

    Outline *item = new Outline;
    d->convertOutline(out, item);
    fz_drop_outline(d->ctx, out);
    return item;
}

fz_context *Document::ctx() const
{
    return d->ctx;
}

fz_document *Document::doc() const
{
    return d->mdoc;
}

float Document::pdfVersion() const
{
    if (!d->mdoc) {
        return 0.0f;
    }

    char buf[64];

    if (fz_lookup_metadata(d->ctx, d->mdoc, FZ_META_FORMAT, buf, sizeof(buf)) != -1) {
        int major, minor;

        if (sscanf(buf, "PDF %d.%d", &major, &minor) == 2) {
            return float(major + minor / 10.0);
        }
    }

    return 0.0f;
}

Document::PageMode Document::pageMode() const
{
    return d->pageMode;
}

/******************************************************************************/

Outline::Outline(const fz_outline *out)
{
    if (out->title) {
        m_title = QString::fromUtf8(out->title);
    }

    if (out->uri) {
        m_link = std::string(out->uri);
    }
}

Outline::~Outline()
{
    qDeleteAll(m_children);
}

} // namespace QMuPDF
