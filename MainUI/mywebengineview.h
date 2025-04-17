#ifndef MYWEBENGINEVIEW_H
#define MYWEBENGINEVIEW_H

#include <QObject>
#include <QWebEngineView>
#include <QWidget>

class myWebEngineView : public QWebEngineView
{
    Q_OBJECT
public:
    myWebEngineView();
    int currentUrlRange = 0;

    void toBack(){
        currentUrlRange--;
        if(currentUrlRange<=0){
            currentUrlRange=0;
        }
        this->load(urlList[currentUrlRange]);
    }

    void toForward(){
        currentUrlRange++;

        if(currentUrlRange>=urlList.length()){
            currentUrlRange = urlList.length()-1;
        }
        this->load(urlList[currentUrlRange]);
    }

    void loadUrl(QString url){
        this->load(url);

        QList<QUrl> temp = urlList;
        urlList.clear();
        for (int var = 0; var < currentUrlRange; ++var) {
            urlList.append(temp[var]);
        }
        urlList.append(url_);
        currentUrlRange++;
    }
protected:
    QWebEngineView* createWindow(QWebEnginePage::WebWindowType type) override
    {
        emit linkClicked(url_);
        this->load(url_);
        return this;
    }

private slots:
    void slot_LinkHovered(const QString& url)
    {
        url_.setUrl(url);
    }

private:
    QUrl url_;
    QList<QUrl> urlList;



signals:
    void linkClicked(QUrl url);
};

#endif // MYWEBENGINEVIEW_H
