#include "decodethread.h"
#include <QDebug>
#include <QDataStream>
#include <qendian.h>

decodeThread::decodeThread(QObject *parent) : QObject(parent)
{

}

void decodeThread::decode(QByteArray data){
    if(data.left(1)==">"){
        QString ID = data.mid(2,1).toHex();
        QString cmd = data.mid(1,1).toHex();

        if(cmd=="12"){
            qDebug()<<data.toHex();
            QByteArray structBody = data.remove(0,5);

            QString motorDrive,motorName,motorID;

            if(structBody.length()>20){
                QString value = structBody.left(20).toHex();
                motorDrive = "motorDrive:"+value+"\n";
                structBody.remove(0,20);
            }else {
                motorDrive = "motorDrive:null\n";
            }

            if(structBody.length()>20){
                QString value = structBody.left(20).toHex();
                motorName = "motorName:"+value+"\n";
                structBody.remove(0,20);
            }else {
                motorName = "motorName:null\n";
            }

            if(structBody.length()>12){
                QString value = structBody.left(20).toHex();
                motorID = "motorID:"+value+"\n";
                structBody.remove(0,12);
            }else {
                motorID = "motorID:null\n";
            }
            emit motorDetails(motorDrive+motorName+motorID);




        }
        int boxLength = data.mid(3,1).toHex().toInt()+6;
        if(boxLength!=data.length())return;



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
        }else if(cmd=="1f"||cmd=="1F"){
            //测试
            emit testOK();
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


