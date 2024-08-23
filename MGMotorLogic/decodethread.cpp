#include "decodethread.h"
#include <QDebug>
#include <QDataStream>
#include <qendian.h>

decodeThread::decodeThread(QObject *parent) : QObject(parent)
{

}

void decodeThread::decode(QByteArray data){
    if(data.left(1)==">"){
        int boxLength = data.mid(3,1).toHex().toInt()+6;
        if(boxLength!=data.length())return;

        QString ID = data.mid(2,1).toHex();
        QString cmd = data.mid(1,1).toHex();

        if(cmd=="92"){
            //位置
            float value = QByteArrayTofloat(data.mid(5,8),"motor");
            currentMotorLength = value;
            emit decodeLengthValue(value);
        }else if(cmd=="9c"||cmd=="9C"){
            //速度
            QByteArray temp = data.mid(9,1)+data.mid(8,1);
            bool ok;
            QByteArray arr(temp.toHex());
            float value = arr.toInt(&ok, 16);
            if(value>65535/2)value = value-65535;
            currentSpeedValue = value;
            emit decodeSpeedValue(value);
        }
    }else{
        int length = data.mid(2,1).toHex().toInt();
        if(data.length()==length+5)
        {
            QString ID = data.mid(0,1).toHex();
            QString cmd = data.mid(1,1).toHex().toUpper();
            float value = QByteArrayTofloat(data.mid(3,4),"weigh");
            if(value>65535||value<-65535)return;
            currentForceValue = value;
            emit decodeForceValue(value);
        }/*else qDebug()<<"unkown"<<data.toHex();*/
    }


}



float decodeThread::QByteArrayTofloat(QByteArray byteArray, QString utils){

    if(utils=="weigh"){
        byteArray = byteArray.right(2)+byteArray.left(2);
        std::reverse(byteArray.begin(), byteArray.end());
    }

    int  outIntVar;
    memcpy(&outIntVar, byteArray.data(), byteArray.length());
    return outIntVar;

}


