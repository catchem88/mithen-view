#include "qvaboutdialog.h"
#include "ui_qvaboutdialog.h"

#include "qvapplication.h"

#include <QJsonDocument>

QVAboutDialog::QVAboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QVAboutDialog)
{
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint | Qt::CustomizeWindowHint));

    connect(ui->checkForUpdatesButton, &QPushButton::clicked, this, &QVAboutDialog::checkForUpdatesButtonClicked);

    // Application modal
    setWindowModality(Qt::WindowModal);

    // add fonts
    qvApp->ensureFontLoaded(":/fonts/Lato-Light.ttf");
    qvApp->ensureFontLoaded(":/fonts/Lato-Regular.ttf");

    int modifier = 0;
    const QFont font1 = QFont("Lato", 72, QFont::Light);
    ui->logoLabel->setFont(font1);

    //set subtitle font & text
    QFont font2 = QFont("Lato", 18 + modifier);
    font2.setStyleName("Regular");
    QString subtitleText = tr("Unofficial Fork (catchem88)") + "<br>";
    subtitleText += tr("Version 1.0.0");
    ui->subtitleLabel->setFont(font2);
    ui->subtitleLabel->setText(subtitleText);

    //set infolabel2 font, text, and properties
    QFont font4 = QFont("Lato", 8 + modifier);
    font4.setStyleName("Regular");
    const QString labelText2 = tr("Built with Qt %1 (%2)<br>"
                                  "Licensed under the GNU GPLv3<br>"
                                  R"(Derivative of unofficial qView (jdpurcell): <a style="color: #03A9F4; text-decoration:none;" href="https://github.com/jdpurcell/qView">GitHub</a><br>)"
                                  R"(Derivative of official qView (jurplel): <a style="color: #03A9F4; text-decoration:none;" href="https://interversehq.com/qview/">Website</a>, <a style="color: #03A9F4; text-decoration:none;" href="https://github.com/jurplel/qView">GitHub</a><br>)"
                                  "Copyright © %3 jurplel, jdpurcell, and wView contributors")
                                  .arg(QT_VERSION_STR, QSysInfo::buildCpuArchitecture(), "2018-2026");

    ui->infoLabel2->setFont(font4);
    ui->infoLabel2->setText(labelText2);

    ui->infoLabel2->setTextInteractionFlags(Qt::TextBrowserInteraction);
    ui->infoLabel2->setOpenExternalLinks(true);

    updateCheckForUpdatesButtonState();
}

QVAboutDialog::~QVAboutDialog()
{
    delete ui;
}

void QVAboutDialog::updateCheckForUpdatesButtonState()
{
    // Update checker removed
}

void QVAboutDialog::checkForUpdatesButtonClicked()
{
    // Update checker removed
}
