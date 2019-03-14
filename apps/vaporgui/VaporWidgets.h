#ifndef VAPORWIDGETS_H 
#define VAPORWIDGETS_H 

#include <QWidget>
#include <QDoubleValidator>

#include "Combo.h"
#include "ui_VPushButton.h"
#include "ui_VComboBox.h"
#include "ui_VCheckBox.h"
#include "ui_VPathSelector.h"

namespace Ui {
class QSliderEdit;
}

class VaporWidget : public QWidget
{
    Q_OBJECT

public:
    VaporWidget( 
        QWidget* parent,
        const std::string& labelText
    );
    VaporWidget( 
        QWidget* parent,
        const QString& labelText
    );
    void SetLabelText( const std::string& text );
    void SetLabelText( const QString& text );

protected:
    QLabel*      _label;
    QSpacerItem* _spacer;
    QHBoxLayout* _layout;
};

class VPushButton: public VaporWidget//QWidget, public Ui_VPushButton
{
    Q_OBJECT

public:
    VPushButton(
        QWidget* parent, 
        std::string labelText = "Label",
        std::string buttonText = "Button"
    );

    void SetButtonText( const std::string& text );
    void SetButtonText( const QString& text );

signals:
    void _pressed();

protected:
    QPushButton* _button;

private slots:
    void _buttonPressed();
};

class VComboBox : public VaporWidget//public QWidget, public Ui_VComboBox
{
    Q_OBJECT

public:
    VComboBox(
        QWidget* parent,
        std::string labelText = "Label"
    );
    int         GetCurrentIndex() const;
    std::string GetCurrentText() const;
    void        AddOption( const std::string& option, int index=0 );
    void        RemoveOption( int index );

private:
    QComboBox* _combo;

private slots:
    void _userIndexChanged(int index);

signals:
    void _indexChanged(int index);
};



class VCheckBox : public VaporWidget//QWidget, public Ui_VCheckBox
{
    Q_OBJECT

public:
    VCheckBox(
        QWidget* parent,
        std::string labelText = "Label"
    );
    bool GetCheckState() const;

private:
    QCheckBox* _checkbox;

private slots:
    void _userClickedCheckbox();

signals:
    void _checkboxClicked();
};

class VPathSelector : public VPushButton//QWidget, public Ui_VPathSelector
{
    Q_OBJECT

public:
    VPathSelector(
        QWidget* parent,
        std::string labelText = "Label",
        std::string filePath = QDir::homePath().toStdString()
    );
    void SetPath( const std::string& defaultPath);
    std::string GetPath() const;

private slots:
    void _openFileDialog();
    void _setFilePath();

signals:
    void _pathChanged();

private:
    QLineEdit* _lineEdit;
    std::string _filePath;
};


#endif // VAPORWIDGETS_H
