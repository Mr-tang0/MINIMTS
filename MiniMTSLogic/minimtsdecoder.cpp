#include "minimtsdecoder.h"
#include <QDebug>

MiniMTSDecoder::MiniMTSDecoder(QObject *parent)
    : QObject{parent}
{


}

uint16_t calculateCRC16(const QByteArray &data)
{
    uint16_t crc = 0xFFFF;

    for (char ch : data) {
        uint8_t currentByte = static_cast<uint8_t>(ch);
        crc ^= currentByte;

        for (int i = 0; i < 8; ++i) {
            uint8_t LSB = crc & 0x0001;
            crc >>= 1;
            if (LSB) {
                crc ^= 0xA001;
            }
        }
    }

    return crc;
}

// 判断最后两个字节是否为 CRC 校验和
bool hasValidCRC16(const QByteArray &data)
{
    if (data.length() < 2) {
        return false;
    }

    // 取出原始数据（除最后两位）
    QByteArray dataWithoutCRC = data.left(data.length() - 2);

    // 计算 CRC
    uint16_t crc = calculateCRC16(dataWithoutCRC);

    // 提取接收到的 CRC（小端格式）
    uint16_t receivedCRC = static_cast<uint8_t>(data[data.length() - 1]) << 8 |
                           static_cast<uint8_t>(data[data.length() - 2]);

    return crc == receivedCRC;
}

void MiniMTSDecoder::decode(QByteArray data){

    //电机回复
    if(data.left(1)==">"){

        QString cmd = data.mid(1,1).toHex();
        if(cmd=="92"&&data.length()==14){
            QByteArray DATA = data.mid(5,8);
            int motorAngle = 0;
            memcpy(&motorAngle, DATA.data(), DATA.length());
            float length = float(motorAngle)/100;

            trueMotorValue = length/motorDetails.resolution;
            currentMotorValue =  trueMotorValue - zeroMotorValue;

            // qDebug()<<"位置"<<DATA.toHex()<<data.toHex()<<currentMotorValue;
        }else if(cmd.toUpper()=="9C"&&data.length()==13){
            QByteArray DATA = data.mid(8,2);
            int speed = 0;
            memcpy(&speed, DATA.data(), DATA.length());
            currentSpeedValue = speed;
        }else if(cmd.toUpper()=="1F"){
            //测试
            emit testOK("motor");
        }
    }
    else if(hasValidCRC16(data)){
        int id = data.mid(0,1).toHex().toInt();
        //称重回复
        if(id==weighDetails.ID.toInt()&&data.length()==9){
            QByteArray DATA = data.mid(3,4);
            int value = 0;
            DATA = DATA.right(2) + DATA.left(2);
            std::reverse(DATA.begin(), DATA.end());
            memcpy(&value, DATA.constData(), sizeof(int));

            currentWeighValue = value/weighDetails.forceRate;

            emit testOK("weigh");
        }
        //限位回复
        else if(id==limitDetails.ID.toInt()){
            int axis =  0;
            if (data.length() == 7){
                qDebug() << "限位：" << axis<<data.toHex()<<id;
                axis = data.mid(4,1).toHex().toInt();

            }else if (data.length() == 9){
                if(data.mid(4,1).toHex().toInt()==1)axis=1;
                else if(data.mid(6,1).toHex().toInt()==1)axis=2;
            }

            emit LimitOccured(axis);
            emit testOK("limit");
        }
    }

}
