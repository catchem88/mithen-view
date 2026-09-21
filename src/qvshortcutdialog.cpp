#include "qvshortcutdialog.h"
#include "ui_qvshortcutdialog.h"
#include "qvapplication.h"

#include <QMessageBox>
#include <QKeyEvent>

#include <QDebug>

QVShortcutDialog::QVShortcutDialog(int index, GetTransientShortcutCallback getTransientShortcutCallback, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QVShortcutDialog)
{
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint | Qt::CustomizeWindowHint));

    connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &QVShortcutDialog::buttonBoxClicked);

    shortcutObject = qvApp->getShortcutManager().getShortcutsList().value(index);
    this->index = index;
    this->getTransientShortcutCallback = getTransientShortcutCallback;
    ui->keySequenceEdit->setKeySequence(getTransientShortcutCallback(index).join(", "));
    ui->keySequenceEdit->setClearButtonEnabled(true);

    //A comma is never part of a shortcut: it separates alternative shortcuts, so intercept it
    ui->keySequenceEdit->installEventFilter(this);
    updateShortcutListLabel();
}

bool QVShortcutDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->keySequenceEdit && event->type() == QEvent::KeyPress)
    {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Comma)
        {
            //Keep the shortcut that was just recorded and start recording the next alternative
            const QString currentSequence = ui->keySequenceEdit->keySequence().toString();
            if (!currentSequence.isEmpty())
            {
                for (const auto &sequence : collectShortcuts(currentSequence))
                {
                    if (!pendingShortcuts.contains(sequence))
                        pendingShortcuts.append(sequence);
                }
                ui->keySequenceEdit->setKeySequence(QKeySequence());
                updateShortcutListLabel();
            }
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

QStringList QVShortcutDialog::collectShortcuts(const QString &keySequenceString) const
{
    QStringList result;
    for (const auto &part : keySequenceString.split(','))
    {
        const QString sequence = part.trimmed();
        if (!sequence.isEmpty() && !result.contains(sequence))
            result.append(sequence);
    }
    return result;
}

void QVShortcutDialog::updateShortcutListLabel()
{
    ui->shortcutListLabel->setText(pendingShortcuts.join(", "));
    ui->shortcutListLabel->setVisible(!pendingShortcuts.isEmpty());
}

QVShortcutDialog::~QVShortcutDialog()
{
    delete ui;
}

void QVShortcutDialog::done(int r)
{
    if (r == QDialog::Accepted)
    {
        return;
    }

    QDialog::done(r);
}

void QVShortcutDialog::buttonBoxClicked(QAbstractButton *button)
{
    if (ui->buttonBox->buttonRole(button) == QDialogButtonBox::AcceptRole)
    {
        QStringList shortcutsStringList = pendingShortcuts;
        for (const auto &sequence : collectShortcuts(ui->keySequenceEdit->keySequence().toString()))
        {
            if (!shortcutsStringList.contains(sequence))
                shortcutsStringList.append(sequence);
        }

        const auto sequenceList = ShortcutManager::stringListToKeySequenceList(shortcutsStringList);

        for (const auto &sequence : sequenceList)
        {
            auto conflictingShortcut = shortcutAlreadyBound(sequence, shortcutObject.name);
            if (!conflictingShortcut.isEmpty())
            {
                QString nativeShortcutString = sequence.toString(QKeySequence::NativeText);
                QMessageBox::warning(this, tr("Shortcut Already Used"), tr("\"%1\" is already bound to \"%2\"").arg(nativeShortcutString, conflictingShortcut));
                return;
            }
        }

        acceptValidated();

        emit shortcutsListChanged(index, shortcutsStringList);
    }
    else if (ui->buttonBox->buttonRole(button) == QDialogButtonBox::ResetRole)
    {
        pendingShortcuts.clear();
        ui->keySequenceEdit->setKeySequence(shortcutObject.defaultShortcuts.join(", "));
        updateShortcutListLabel();
    }
}

QString QVShortcutDialog::shortcutAlreadyBound(const QKeySequence &chosenSequence, const QString &exemptShortcut)
{
    if (chosenSequence.isEmpty())
        return "";

    const auto &shortcutsList = qvApp->getShortcutManager().getShortcutsList();
    for (int i = 0; i < shortcutsList.length(); i++)
    {
        const auto &shortcut = shortcutsList.value(i);
        const auto sequenceList = ShortcutManager::stringListToKeySequenceList(getTransientShortcutCallback(i));

        if (sequenceList.contains(chosenSequence) && shortcut.name != exemptShortcut)
            return shortcut.readableName;
    }
    return "";
}

void QVShortcutDialog::acceptValidated()
{
    QDialog::done(1);
}
