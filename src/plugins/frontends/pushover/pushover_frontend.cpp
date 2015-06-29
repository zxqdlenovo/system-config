/*
    SnoreNotify is a Notification Framework based on Qt
    Copyright (C) 2013-2014  Patrick von Reth <vonreth@kde.org>

    SnoreNotify is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    SnoreNotify is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with SnoreNotify.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "pushover_frontend.h"
#include "pushoversettings.h"

#include "libsnore/snore.h"
#include "libsnore/version.h"
#include "libsnore/notification/notification_p.h"


#include <QNetworkAccessManager>
#include <QHttpMultiPart>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtWebSockets/QWebSocket>


using namespace Snore;

// TODO: use qtkeychain to encrypt credentials?
// TODO: massive refactoring ...

bool PushoverFrontend::initialize()
{
    setDefaultValue("Secret", "", LOCAL_SETTING);
    setDefaultValue("DeviceID", "", LOCAL_SETTING);
    setDefaultValue("Registered", false, LOCAL_SETTING);

    if(!SnoreFrontend::initialize()) {
        return false;
    }

    if(value("Registered", LOCAL_SETTING).toBool())
    {
        connectToService();
    }

    return true;
}

bool PushoverFrontend::deinitialize()
{
    if (SnoreFrontend::deinitialize()) {
        if(m_socket) {
            m_socket->close();
            m_socket->deleteLater();
        }
        return true;
    }
    return false;
}

PluginSettingsWidget *PushoverFrontend::settingsWidget()
{
    return new PushoverSettings(this);
}

void PushoverFrontend::registerDevice(const QString &email, const QString &password, const QString &deviceName)
{

    QNetworkRequest request(QUrl(QStringLiteral("https://api.pushover.net/1/users/login.json")));

    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));
    QNetworkReply *reply = m_manager.post(request, QString("email=%1&password=%2").arg(email, password).toUtf8().constData());


    connect(reply, &QNetworkReply::finished, [reply, deviceName, this]() {
        snoreDebug(SNORE_DEBUG) << reply->error();
        QByteArray input = reply->readAll();
        reply->close();
        reply->deleteLater();


        QJsonObject message = QJsonDocument::fromJson(input).object();

        if(message.value("status").toInt() == 1)
        {
            QString secret = message.value("secret").toString();
            QNetworkRequest request(QUrl(QStringLiteral("https://api.pushover.net/1/devices.json")));

            request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));
            QNetworkReply *reply = m_manager.post(request, QString("secret=%1&name=%2&os=O").arg(secret, deviceName).toUtf8().constData());


            connect(reply, &QNetworkReply::finished, [reply, secret, this]() {
                snoreDebug(SNORE_DEBUG) << reply->error();
                QByteArray input = reply->readAll();
                reply->close();
                reply->deleteLater();


                QJsonObject message = QJsonDocument::fromJson(input).object();


                if(message.value("status").toInt() == 1) {
                    setValue("Secret", secret, LOCAL_SETTING);
                    setValue("DeviceID", message.value("id").toString(), LOCAL_SETTING);
                    setValue("Registered", true, LOCAL_SETTING);
                    connectToService();
                } else {
                    snoreDebug(SNORE_WARNING) << "An error occure" << input;
                }

            });
        }else {
            snoreDebug(SNORE_WARNING) << "An error occure" << input;
        }
    });
}

void PushoverFrontend::slotActionInvoked(Notification notification)
{
    if(notification.priority() == Notification::EMERGENCY){
        snoreDebug(SNORE_WARNING) << "emergeency notification" << notification;
        acknowledgeNotification(notification);
    }
}

QString PushoverFrontend::secret()
{
    return value("Secret",  LOCAL_SETTING).toString();
}

QString PushoverFrontend::device()
{
    return value("DeviceID",  LOCAL_SETTING).toString();
}

void PushoverFrontend::connectToService()
{

    if(!value("Registered", LOCAL_SETTING).toBool())
    {
        snoreDebug(SNORE_WARNING) << "not logged in";
        return;
    }
    m_socket = new QWebSocket("", QWebSocketProtocol::VersionLatest, this);

    connect(m_socket, &QWebSocket::binaryMessageReceived, [&](const QByteArray &msg){
        char c = msg.at(0);
        switch(c){
        case '#':
            snoreDebug(SNORE_DEBUG) << "still alive";
            break;
        case '!':
            getMessages();
            break;
        case 'R':
            snoreDebug(SNORE_DEBUG) << "need to reconnect";
            m_socket->close();
            m_socket->deleteLater();
            connectToService();
            break;
        case 'E':
            snoreDebug(SNORE_DEBUG) << "Connection Error";
            setValue("Registered", false, LOCAL_SETTING);
            m_socket->close();
            m_socket->deleteLater();
            break;
        default:
            snoreDebug(SNORE_WARNING) << "unknown message recieved" << msg;
        }
    });
    connect(m_socket, &QWebSocket::disconnected, [](){
        snoreDebug(SNORE_DEBUG) << "disconnected";
    });
    connect(m_socket, static_cast<void (QWebSocket::*)(QAbstractSocket::SocketError)>(&QWebSocket::error), [&](QAbstractSocket::SocketError error){
        snoreDebug(SNORE_DEBUG) << error << m_socket->errorString();
    });
    connect(m_socket, &QWebSocket::connected, [&](){
        snoreDebug(SNORE_DEBUG) << "connecting";
        m_socket->sendBinaryMessage(QString("login:%1:%2\n").arg(device(), secret()).toUtf8().constData());
        getMessages();
    });
    m_socket->open(QUrl("wss://client.pushover.net/push"));
}

void PushoverFrontend::getMessages()
{
    QNetworkRequest request(QUrl(QString("https://api.pushover.net/1/messages.json?"
                                         "secret=%1&device_id=%2").arg(secret(), device())));
    QNetworkReply *reply =  m_manager.get(request);


    connect(reply, &QNetworkReply::finished, [reply,this]() {
        snoreDebug(SNORE_DEBUG) << reply->error();
        QByteArray input = reply->readAll();
        reply->close();
        reply->deleteLater();

        QJsonObject message = QJsonDocument::fromJson(input).object();

        int latestID = -1;
        if(message.value("status").toInt() == 1){
            QJsonArray notifications = message.value("messages").toArray();
            for( const QJsonValue &v : notifications) {
                QJsonObject notification = v.toObject();

                latestID = notification.value("id").toInt();

                QString appName = notification.value("app").toString();
                Application app = SnoreCore::instance().aplications().value(appName);

                if (!app.isValid()){
                    Icon icon(QString("https://api.pushover.net/icons/%1.png").arg(notification.value("icon").toString()));
                    app = Application(appName, icon);
                    app.addAlert(Alert("Default", icon));
                    if(notification.value("html").toInt() == 1) {
                        app.hints().setValue("use-markup", QVariant::fromValue(true)) ;
                    }
                    SnoreCore::instance().registerApplication(app);
                }


                Notification n(app, *app.alerts().begin(), notification.value("title").toString(), notification.value("message").toString(),
                               app.icon(), Notification::defaultTimeout(), static_cast<Notification::Prioritys>(notification.value("priority").toInt()));
                if(n.priority() == Notification::EMERGENCY){
                    n.hints().setValue("receipt", notification.value("receipt").toString());
                }
                SnoreCore::instance().broadcastNotification(n);
            }
            if(latestID != -1){
                deleteMessages(latestID);
            }
        } else {
            snoreDebug(SNORE_WARNING) << "An error occure" << input;
        }

    });

}

void PushoverFrontend::deleteMessages(int latestMessageId)
{

    QNetworkRequest request(QUrl(QString("https://api.pushover.net/1/devices/%1/update_highest_message.json").arg(device())));

    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));
    QNetworkReply *reply = m_manager.post(request, QString("secret=%1&message=%2").arg(secret(), QString::number(latestMessageId)).toUtf8().constData());


    connect(reply, &QNetworkReply::finished, [reply]() {
        snoreDebug(SNORE_DEBUG) << reply->error();
        snoreDebug(SNORE_DEBUG) << reply->readAll();
        reply->close();
        reply->deleteLater();
    });
}

void PushoverFrontend::acknowledgeNotification(Notification notification)
{
    QString receipt = notification.constHints().value("receipt").toString();

    QNetworkRequest request(QUrl(QString("https://api.pushover.net/1/receipts/%1/acknowledge.json").arg(receipt)));

    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));
    QNetworkReply *reply = m_manager.post(request, QString("secret=%1").arg(secret()).toUtf8().constData());


    connect(reply, &QNetworkReply::finished, [reply]() {
        snoreDebug(SNORE_DEBUG) << reply->error();
        snoreDebug(SNORE_DEBUG) << reply->readAll();
        reply->close();
        reply->deleteLater();
    });
}


