#include "qvaboutdialog.h"
#include "ui_qvaboutdialog.h"

#include "qvapplication.h"

#include <QPixmap>

QVAboutDialog::QVAboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QVAboutDialog)
{
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint | Qt::CustomizeWindowHint));

    // Application modal
    setWindowModality(Qt::WindowModal);

    // add fonts
    qvApp->ensureFontLoaded(":/fonts/Lato-Light.ttf");
    qvApp->ensureFontLoaded(":/fonts/Lato-Regular.ttf");

    int modifier = 0;

    //show the MithenView logo
    ui->logoLabel->setPixmap(QPixmap(":/logo.png").scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    //set subtitle font & text
    QFont font2 = QFont("Lato", 18 + modifier);
    font2.setStyleName("Regular");
    QString subtitleText = tr("MithenView v%1").arg(QString::fromLatin1(MITHEINVIEW_VERSION));
    ui->subtitleLabel->setFont(font2);
    ui->subtitleLabel->setText(subtitleText);

    //set infolabel2 font, text, and properties
    QFont font4 = QFont("Lato", 8 + modifier);
    font4.setStyleName("Regular");
    const QString labelText2 = tr("Built with Qt %1 (%2)<br>"
                                  "Licensed under the GNU GPLv3<br>"
                                  R"(Project: <a style="color: #03A9F4; text-decoration:none;" href="https://github.com/catchem88/mithen-view">GitHub</a><br>)"
                                  R"(Derivative of unofficial qView (jdpurcell): <a style="color: #03A9F4; text-decoration:none;" href="https://github.com/jdpurcell/qView">GitHub</a><br>)"
                                  R"(Derivative of official qView (jurplel): <a style="color: #03A9F4; text-decoration:none;" href="https://interversehq.com/qview/">Website</a>, <a style="color: #03A9F4; text-decoration:none;" href="https://github.com/jurplel/qView">GitHub</a><br>)"
                                  "Copyright © %3 jurplel, jdpurcell, and MithenApps")
                                  .arg(QT_VERSION_STR, QSysInfo::buildCpuArchitecture(), "2018-2026");

    ui->infoLabel2->setFont(font4);
    ui->infoLabel2->setText(labelText2);

    ui->infoLabel2->setTextInteractionFlags(Qt::TextBrowserInteraction);
    ui->infoLabel2->setOpenExternalLinks(true);
}

QVAboutDialog::~QVAboutDialog()
{
    delete ui;
}
