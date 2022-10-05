/***************************************************************************
 *   Copyright (C) 2008 by Pino Toscano <pino@kde.org>                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "generator_mupdf.hpp"
#include "page.hpp"

#include <okular/core/page.h>
#include <okular/core/textpage.h>

#include <KLocalizedString>

#include <QFile>
#include <QImage>
#include <QMutexLocker>

OKULAR_EXPORT_PLUGIN(MuPDFGenerator, "libokularGenerator_mupdf.json")

MuPDFGenerator::MuPDFGenerator(QObject *parent, const QVariantList &args)
    : Generator(parent, args)
    , m_synopsis(nullptr)
{
    setFeature(Threaded);
    setFeature(TextExtraction);
}

MuPDFGenerator::~MuPDFGenerator() = default;

Okular::Document::OpenResult MuPDFGenerator::loadDocumentWithPassword(
    const QString &fileName, QVector<Okular::Page *> &pages,
    const QString &password)
{
    if (!m_pdfdoc.load(fileName)) {
        return Okular::Document::OpenError;
    }

    if (m_pdfdoc.isLocked()) {
        m_pdfdoc.unlock(password.toLocal8Bit());

        if (m_pdfdoc.isLocked()) {
            m_pdfdoc.close();
            return Okular::Document::OpenNeedsPassword;
        }
    }

    for (int i = 0; i < m_pdfdoc.pageCount(); ++i) {
        QMuPDF::Page page = m_pdfdoc.page(i);
        const QSizeF s = page.size(dpi());
        const Okular::Rotation rot = Okular::Rotation0;
        Okular::Page *okularPage = new Okular::Page(i, s.width(), s.height(), rot);
        okularPage->setDuration(page.duration());
        pages.append(okularPage);
    }

    return Okular::Document::OpenSuccess;
}

bool MuPDFGenerator::doCloseDocument()
{
    QMutexLocker locker(userMutex());
    m_pdfdoc.close();
    delete m_synopsis;
    m_synopsis = nullptr;
    return true;
}

Okular::DocumentInfo MuPDFGenerator::generateDocumentInfo(const QSet<Okular::DocumentInfo::Key> &keys) const
{
    QMutexLocker locker(userMutex());
    Okular::DocumentInfo info;
    info.set(Okular::DocumentInfo::MimeType, QStringLiteral("application/pdf"));
    info.set(Okular::DocumentInfo::Pages, QString::number(m_pdfdoc.pageCount()));
#define SET(key, val) if (keys.contains(key)) { info.set(key, val); }
    SET(Okular::DocumentInfo::Title, m_pdfdoc.infoKey("Title"));
    SET(Okular::DocumentInfo::Subject, m_pdfdoc.infoKey("Subject"));
    SET(Okular::DocumentInfo::Author, m_pdfdoc.infoKey("Author"));
    SET(Okular::DocumentInfo::Keywords, m_pdfdoc.infoKey("Keywords"));
    SET(Okular::DocumentInfo::Creator, m_pdfdoc.infoKey("Creator"));
    SET(Okular::DocumentInfo::Producer, m_pdfdoc.infoKey("Producer"));
#undef SET

    if (keys.contains(Okular::DocumentInfo::CustomKeys)) {
        info.set(QStringLiteral("format"), i18nc("PDF v. <version>", "PDF v. %1", m_pdfdoc.pdfVersion()), i18n("Format"));
    }

    return info;
}

static void recurseCreateTOC(const QMuPDF::Document &doc, QDomDocument &mainDoc, QMuPDF::Outline *outline,
                             QDomNode &parentDestination, const QSizeF &dpi)
{
    foreach (QMuPDF::Outline *child, outline->children()) {
        QDomElement newel = mainDoc.createElement(child->title());
        parentDestination.appendChild(newel);

        if (child->isOpen()) {
            newel.setAttribute(QStringLiteral("Open"), QStringLiteral("true"));
        }

        std::string link = child->link();

        if (!link.size()) {
            continue;
        }

        if (fz_is_external_link(doc.ctx(), link.c_str())) {
            newel.setAttribute(QStringLiteral("DestinationURI"), QString::fromUtf8(link.c_str()));
        } else {
            float xp = 0, yp = 0;
            fz_location location = fz_resolve_link(doc.ctx(), doc.doc(), link.c_str(), &xp, &yp);

            if (location.page == -1) {
                continue;
            }

            Okular::DocumentViewport vp(location.page);
            vp.rePos.pos = Okular::DocumentViewport::TopLeft;
            vp.rePos.normalizedX = xp;
            vp.rePos.normalizedY = yp;
            vp.rePos.enabled = true;
            newel.setAttribute(QStringLiteral("Viewport"), vp.toString());
        }

        recurseCreateTOC(doc, mainDoc, child, newel, dpi);
    }
}

const Okular::DocumentSynopsis *MuPDFGenerator::generateDocumentSynopsis()
{
    QMutexLocker locker(userMutex());

    if (m_synopsis) {
        return m_synopsis;
    }

    QMuPDF::Outline *outline = m_pdfdoc.outline();

    if (!outline) {
        return nullptr;
    }

    m_synopsis = new Okular::DocumentSynopsis();
    recurseCreateTOC(m_pdfdoc, *m_synopsis, outline, *m_synopsis, dpi());
    delete outline;
    return m_synopsis;
}

QImage MuPDFGenerator::image(Okular::PixmapRequest *request)
{
    QMutexLocker locker(userMutex());
    QMuPDF::Page page = m_pdfdoc.page(request->page()->number());
    QImage image = page.render(request->width(), request->height());
    return image;
}

static Okular::TextPage *buildTextPage(const QVector<QMuPDF::TextBox *> &boxes,
                                       qreal width, qreal height)
{
    Okular::TextPage *ktp = new Okular::TextPage();

    for (int i = 0; i < boxes.size(); ++i) {
        QMuPDF::TextBox *box = boxes.at(i);
        const QChar c = box->text();
        const QRectF charBBox = box->rect();
        QString text(c);

        if (box->isAtEndOfLine()) {
            text.append(QLatin1Char('\n'));
        }

        ktp->append(text, new Okular::NormalizedRect(
                        charBBox.left() / width, charBBox.top() / height,
                        charBBox.right() / width, charBBox.bottom() / height));
    }

    return ktp;
}

Okular::TextPage *MuPDFGenerator::textPage(Okular::TextRequest *request)
{
    QMutexLocker locker(userMutex());
    QMuPDF::Page mp = m_pdfdoc.page(request->page()->number());
    const QVector<QMuPDF::TextBox *> boxes = mp.textBoxes(dpi());
    const QSizeF s = mp.size(dpi());
    Okular::TextPage *tp = buildTextPage(boxes, s.width(), s.height());
    qDeleteAll(boxes);
    return tp;
}

QVariant MuPDFGenerator::metaData(const QString &key,
                                  const QVariant &option) const
{
    Q_UNUSED(option)

    if (key == QStringLiteral("NamedViewport") && !option.toString().isEmpty()) {
        qWarning() << "We don't store named viewports properly, but it asked for" << option.toString();
    } else if (key == QLatin1String("DocumentTitle")) {
        QMutexLocker locker(userMutex());
        const QString title = m_pdfdoc.infoKey("Title");
        return title;
    } else if (key == QLatin1String("StartFullScreen")) {
        if (m_pdfdoc.pageMode() == QMuPDF::Document::FullScreen) {
            return true;
        }
    } else if (key == QLatin1String("OpenTOC")) {
        if (m_pdfdoc.pageMode() == QMuPDF::Document::UseOutlines) {
            return true;
        }
    }

    return QVariant();
}

#include "generator_mupdf.moc"
