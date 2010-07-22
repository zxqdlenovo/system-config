/****************************************************************************************
 * Copyright (c) 2010 Patrick von Reth <patrick.vonreth@gmail.com>                      *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#include "application.h"


Application::Application(const QString &name):
        _name(name),
        _initialized(false)
{
    _alerts.insert("",new Alert("Default Alert","Default Alert"));
}

Application::Application():
        _name("Error: Uninitialized Application")
{}

Application::~Application(){
    foreach(Alert *a,_alerts){
        a->deleteLater();
    }
}

void Application::addAlert(Alert *alert)
{
    _alerts.insert(alert->name(),alert);
}

const QString &Application::name() const{
    return _name;
}

const AlertList &Application::alerts() const{
    return _alerts;
}

bool Application::isInitialized(){
    return _initialized;
}

void Application::setInitialized(bool b){
    _initialized = b;
}

Alert::Alert(const QString &name,const QString &title):
        _name(name),
        _title(title),
        _active(true)
{}

Alert::Alert(const QString &name,const QString &title,bool active):
        _name(name),
        _title(title),
        _active(active)
{}

Alert::Alert():
        _active(false)
{}


const QString &Alert::name() const{
    return _name;
}

const QString &Alert::title() const{
    return _title;
}

bool Alert::isActive() const{
    return _active;
}

#include "application.moc"



