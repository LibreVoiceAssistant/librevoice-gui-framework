#include "audiorec.h"
#include "controllerconfig.h"

#include <QUrl>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QAudioBuffer>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QIODevice>
#include <QSysInfo>

AudioRec::AudioRec(QObject *parent) :
    QObject(parent),
    m_controller(MycroftController::instance())
{

}

void AudioRec::recordTStart()
{
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open( QIODevice::ReadWrite | QIODevice::Truncate );
    QAudioFormat format;
    format.setSampleRate(8000);
    format.setChannelCount(1);
    // format.setSampleSize(8);
    // format.setCodec(QStringLiteral("audio/pcm"));
    // format.setByteOrder(QSysInfo::LittleEndian);
    format.setSampleFormat(QAudioFormat::UInt8);

    QAudioDevice info = QMediaDevices::defaultAudioInput();
    if (!info.isFormatSupported(format)) {
        qWarning() << "Format is not supported.";
        format = info.preferredFormat();
    }

    audio = new QAudioSource(format, this);
    device = audio->start();
    destinationFile.setFileName(QStringLiteral("/tmp/mycroft_in.raw"));
    destinationFile.open( QIODevice::WriteOnly | QIODevice::Truncate );
    connect(device, &QIODevice::readyRead, this, &AudioRec::captureDataFromDevice);
}

void AudioRec::recordTStop()
{
    audio->stop();
    destinationFile.close();
    emit recordTStatus(QStringLiteral("Completed"));
}

void AudioRec::readStream()
{
    QFile inputFile;
    QDir::setCurrent(QStringLiteral("/tmp"));
    inputFile.setFileName(QStringLiteral("mycroft_in.raw"));
    m_audStream_size = inputFile.size();
    qDebug() << "filesize: " << m_audStream_size;
    inputFile.open(QIODevice::ReadWrite);
    while(!inputFile.atEnd()) {
      m_audStream = inputFile.readAll();
    }
    inputFile.close();
}

void AudioRec::returnStream()
{
    QJsonObject dataObject;
    QByteArray utteranceArray;
    utteranceArray.push_front(m_audStream.toHex());
    dataObject.insert(QStringLiteral("lang"), QStringLiteral("en-us"));
    dataObject.insert(QStringLiteral("audio"), QJsonValue::fromVariant(utteranceArray));
    m_controller->sendBinary(QStringLiteral("recognizer_loop:incoming_aud"), dataObject);
}

void AudioRec::captureDataFromDevice()
{
    QByteArray inputByteArray = device->readAll();
    destinationFile.write(inputByteArray);
    const int channelbytes = (audio->format().sampleRate() * audio->format().channelCount()) / 8;
    const int samplecount = inputByteArray.size() / channelbytes;
    quint32 maxvalue= 0;
    quint32 value = 0;
    quint32 amp = 255;
    const unsigned char* ptr = reinterpret_cast<const unsigned char*>(inputByteArray.data());
    for (int i = 0; i < samplecount; ++i) {
        for (int j = 0; j < audio->format().channelCount(); ++j) {
            value = *reinterpret_cast<const quint8*>(ptr);
        }
        maxvalue = qMax(value, maxvalue);
        ptr += channelbytes;
    }
    maxvalue = qMin(maxvalue, amp);
    qreal m_level = qreal(maxvalue) / amp;
    emit micAudioLevelChanged(m_level);
}
