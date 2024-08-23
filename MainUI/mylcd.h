#ifndef MYLCD_H
#define MYLCD_H

#include <QWidget>
#include <QMouseEvent>

namespace Ui {
class MyLCD;
}

class MyLCD : public QWidget
{
    Q_OBJECT

public:
    explicit MyLCD(QWidget *parent = nullptr);
    ~MyLCD();
    void setBackground();
    void setKey(QString);
    void display(float value);
    void setFont(int font);
    void setDecimals(int decimal);

    float currentValue();
    QString currentKey();

private:
    Ui::MyLCD *ui;
    int decimals = 2;

public slots:
    void doubleClick(){
        emit doubleClicked();
    }

protected:
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::MouseButtonDblClick)
        {
            emit doubleClicked();
            return true;
        }
        return QWidget::event(event);
    }
signals:
    void doubleClicked();
};

#endif // MYLCD_H
