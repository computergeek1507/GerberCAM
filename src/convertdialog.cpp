#include "convertdialog.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>

#include <memory>

#include "setting.h"
#include "gerber.h"
#include "dxfexport.h"
#include "svgexport.h"

namespace {

// Layer types selectable in the dialog. dxfLayer must exist in the DXF
// skeleton layer table (res/dxf_skeleton_prefix.txt).
struct LayerType
{
    const char *label;
    const char *dxfLayer;
    const char *svgColor;
};

const LayerType kLayerTypes[] = {
    { "Top copper",         "TOP_COPPER",    "#c83232" },
    { "Bottom copper",      "BOTTOM_COPPER", "#3264c8" },
    { "Top silkscreen",     "TOP_SILK",      "#c88c00" },
    { "Bottom silkscreen",  "BOTTOM_SILK",   "#9032c8" },
    { "Top solder mask",    "TOP_MASK",      "#207820" },
    { "Bottom solder mask", "BOTTOM_MASK",   "#20a090" },
};

// Row helper: line edit plus a Browse button.
QWidget *pathRow(QWidget *parent, QLineEdit *&edit, const char *slotName,
                 ConvertDialog *dlg)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    edit = new QLineEdit(row);
    auto *browse = new QPushButton(ConvertDialog::tr("Browse..."), row);
    QObject::connect(browse, SIGNAL(clicked()), dlg, slotName);
    layout->addWidget(edit, 1);
    layout->addWidget(browse);
    return row;
}

} // namespace

ConvertDialog::ConvertDialog(Setting *settings, QWidget *parent)
    : QDialog(parent), m_settings(settings)
{
    setWindowTitle(tr("Convert Gerber to DXF/SVG"));
    setMinimumWidth(520);

    auto *form = new QFormLayout(this);

    form->addRow(tr("Gerber file:"),
                 pathRow(this, m_gerberEdit, SLOT(browseGerber()), this));
    form->addRow(tr("Outline gerber:"),
                 pathRow(this, m_outlineEdit, SLOT(browseOutline()), this));

    m_layerCombo = new QComboBox(this);
    for (const LayerType &t : kLayerTypes)
        m_layerCombo->addItem(tr(t.label));
    form->addRow(tr("Layer type:"), m_layerCombo);

    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem("DXF");
    m_formatCombo->addItem("SVG");
    form->addRow(tr("Output format:"), m_formatCombo);
    connect(m_formatCombo, &QComboBox::currentTextChanged, this, [this]()
    {
        // Keep an already-chosen output path in sync with the format.
        QString out = m_outputEdit->text();
        if (out.endsWith(".dxf", Qt::CaseInsensitive)
            || out.endsWith(".svg", Qt::CaseInsensitive))
        {
            out.chop(4);
            m_outputEdit->setText(out + formatExtension());
        }
    });

    m_flipCheck = new QCheckBox(tr("Flip horizontally (mirror X)"), this);
    form->addRow(QString(), m_flipCheck);

    form->addRow(tr("Output file:"),
                 pathRow(this, m_outputEdit, SLOT(browseOutput()), this));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto *convertBtn = buttons->addButton(tr("Convert"),
                                          QDialogButtonBox::ActionRole);
    convertBtn->setDefault(true);
    connect(convertBtn, &QPushButton::clicked, this, &ConvertDialog::convert);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    form->addRow(buttons);
}

void ConvertDialog::browseGerber()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("Select Gerber File"), m_settings->lastDir(),
        tr("Gerber Files (*.gtl *.gbl *.gto *.gbo *.gts *.gbs *.gbr *.ger *.pho);;All types (*.*)"));
    if (path.isEmpty())
        return;
    m_settings->setLastDir(path);
    m_gerberEdit->setText(path);
    guessLayerType(path);
    suggestOutput(path);
}

void ConvertDialog::guessLayerType(const QString &path)
{
    const QString name = QFileInfo(path).fileName().toLower();
    const bool bottom = name.contains("b_") || name.contains("b.")
                     || name.contains("bottom");

    int index = -1;
    if (name.endsWith(".gto") || (name.contains("silk") && !bottom))
        index = 2; // top silkscreen
    else if (name.endsWith(".gbo") || (name.contains("silk") && bottom))
        index = 3; // bottom silkscreen
    else if (name.endsWith(".gts") || (name.contains("mask") && !bottom))
        index = 4; // top solder mask
    else if (name.endsWith(".gbs") || (name.contains("mask") && bottom))
        index = 5; // bottom solder mask
    else if (name.endsWith(".gbl") || name.contains("b_cu")
             || name.contains("b.cu") || name.contains("bottom"))
        index = 1; // bottom copper
    else if (name.endsWith(".gtl") || name.contains("f_cu")
             || name.contains("f.cu") || name.contains("top"))
        index = 0; // top copper

    if (index >= 0)
        m_layerCombo->setCurrentIndex(index);
}

void ConvertDialog::browseOutline()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("Select Outline / Edge Cuts Gerber"), m_settings->lastDir(),
        tr("Edge Cuts (*.gm1 *.gko *.gm);;Gerber Files (*.gbr *.ger);;All types (*.*)"));
    if (path.isEmpty())
        return;
    m_settings->setLastDir(path);
    m_outlineEdit->setText(path);
    suggestOutput(path);
}

void ConvertDialog::browseOutput()
{
    const QString ext = formatExtension();
    QString start = m_outputEdit->text();
    if (start.isEmpty())
        start = m_settings->lastDir();
    QString path = QFileDialog::getSaveFileName(
        this, tr("Output File"), start,
        tr("%1 files (*%2);;All files (*)")
            .arg(m_formatCombo->currentText(), ext));
    if (path.isEmpty())
        return;
    if (!path.endsWith(ext, Qt::CaseInsensitive))
        path += ext;
    m_settings->setLastDir(path);
    m_outputEdit->setText(path);
}

void ConvertDialog::suggestOutput(const QString &inputPath)
{
    if (!m_outputEdit->text().isEmpty())
        return;
    QFileInfo fi(inputPath);
    m_outputEdit->setText(fi.absolutePath() + "/" + fi.completeBaseName()
                          + formatExtension());
}

QString ConvertDialog::formatExtension() const
{
    return m_formatCombo->currentIndex() == 1 ? ".svg" : ".dxf";
}

void ConvertDialog::convert()
{
    const QString gerberPath  = m_gerberEdit->text().trimmed();
    const QString outlinePath = m_outlineEdit->text().trimmed();
    QString outputPath        = m_outputEdit->text().trimmed();

    if (gerberPath.isEmpty() && outlinePath.isEmpty())
    {
        QMessageBox::warning(this, windowTitle(),
            tr("Select a gerber and/or an outline gerber file."));
        return;
    }
    if (outputPath.isEmpty())
    {
        QMessageBox::warning(this, windowTitle(),
            tr("Select an output file."));
        return;
    }
    if (!outputPath.endsWith(formatExtension(), Qt::CaseInsensitive))
        outputPath += formatExtension();

    std::unique_ptr<Gerber> artwork;
    if (!gerberPath.isEmpty())
    {
        artwork = std::make_unique<Gerber>(gerberPath);
        if (!artwork->readingFlag)
        {
            QMessageBox::critical(this, windowTitle(),
                tr("Failed to read gerber:\n%1").arg(gerberPath));
            return;
        }
    }

    std::unique_ptr<Gerber> outline;
    if (!outlinePath.isEmpty())
    {
        outline = std::make_unique<Gerber>(outlinePath);
        if (!outline->readingFlag)
        {
            QMessageBox::critical(this, windowTitle(),
                tr("Failed to read outline gerber:\n%1").arg(outlinePath));
            return;
        }
    }

    const bool asSvg = (m_formatCombo->currentIndex() == 1);
    const LayerType &type = kLayerTypes[m_layerCombo->currentIndex()];
    const bool flip = m_flipCheck->isChecked();

    bool ok;
    QString errorMsg;
    if (artwork)
    {
        // Artwork layer (with the outline overlaid when one is given).
        ok = asSvg
            ? SvgExport::writeCopper(*artwork, type.svgColor,
                                     outputPath, errorMsg, flip, outline.get())
            : DxfExport::writeCopper(*artwork, type.dxfLayer,
                                     outputPath, errorMsg, flip, outline.get());
    }
    else
    {
        // Outline only.
        ok = asSvg
            ? SvgExport::writeOutline(*outline, outputPath, errorMsg, flip)
            : DxfExport::writeOutline(*outline, outputPath, errorMsg, flip);
    }

    if (ok)
        QMessageBox::information(this, windowTitle(),
            tr("Converted successfully:\n%1").arg(outputPath));
    else
        QMessageBox::critical(this, windowTitle(),
            tr("Conversion failed:\n%1").arg(errorMsg));
}
