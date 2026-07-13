#pragma once

#include <QDialog>

class QLineEdit;
class QComboBox;
class QCheckBox;
class Setting;

// Standalone Gerber → DXF/SVG converter. Select any artwork gerber (copper,
// silkscreen, solder mask, ...) and/or an outline gerber and write a single
// DXF or SVG file, independent of the board loaded in the main window.
class ConvertDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConvertDialog(Setting *settings, QWidget *parent = nullptr);

private slots:
    void browseGerber();
    void browseOutline();
    void browseOutput();
    void convert();

private:
    // Default the output path next to the given input when it is empty.
    void suggestOutput(const QString &inputPath);
    // Pick the layer type combo entry matching a file name.
    void guessLayerType(const QString &path);
    QString formatExtension() const;

    Setting *m_settings;
    QLineEdit *m_gerberEdit;
    QLineEdit *m_outlineEdit;
    QLineEdit *m_outputEdit;
    QComboBox *m_layerCombo;   // copper / silkscreen / mask, top / bottom
    QComboBox *m_formatCombo;  // DXF / SVG
    QCheckBox *m_flipCheck;
};
