/* Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "MaterialMotionAuthoring.h"
#include "MaterialElementOverrides.h"
#include "MaterialMotion.h"
#include "MaterialVoice.h"
#include <QRegularExpression>
#include <QAbstractButton>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QEasingCurve>
#include <cmath>
#include <QSet>
#include <QHideEvent>
namespace Material {
    QString motionCopy(const QString& english) {
        QString id=english.toLower();
        id.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")),QStringLiteral("_"));
        while(id.startsWith(QLatin1Char('_')))id.remove(0,1);
        while(id.endsWith(QLatin1Char('_')))id.chop(1);
        const auto key=QStringLiteral("motion.authoring.")+id.left(100);
        return Voice::catalogueKeys().contains(key)?Voice::say(key):english;
    }
    namespace {
        QSet<MotionPlayer*> activePlayers;
        // GUI-thread only, bounded across live targets and previews.
        bool number(const QJsonObject& o, const char* key, double lo, double hi) {
            auto v=o.value(QLatin1String(key));
            return v.isDouble() && std::isfinite(v.toDouble()) && v.toDouble()>=lo && v.toDouble()<=hi;
        }
        QPair<double,double> bounds(const QString& p) {
            if(p==QLatin1String("height")) return {
                24,160
            };
            if(p==QLatin1String("radius")) return {
                0,80
            };
            if(p==QLatin1String("fontSize")) return {
                8,48
            };
            if(p==QLatin1String("spacing")) return {
                0,48
            };
            return {
                0,8
            };
        }
        QStringList curves() {
            return {
                QStringLiteral("linear"),QStringLiteral("inOutCubic"),QStringLiteral("outCubic")
            };
        }
        QJsonObject track()
        {
            QJsonObject t{{"name",QStringLiteral("Track")},{"state",QStringLiteral("normal")},
                          {"property",QStringLiteral("radius")},{"duration",1000},{"delay",0},
                          {"easing",QStringLiteral("linear")},{"enabled",true},{"mute",false},{"solo",false}};
            t["keyframes"]=QJsonArray{QJsonObject{{"offset",0.0},{"value",0.0}},
                                      QJsonObject{{"offset",1.0},{"value",24.0}}};
            return t;
        }
    }
    QStringList MotionDocument::properties() {
        return {
            QStringLiteral("height"),QStringLiteral("radius"),QStringLiteral("fontSize"),QStringLiteral("spacing"),QStringLiteral("borderWidth")
        };
    }
    QJsonObject MotionDocument::empty() {
        return {
            {
                "schemaVersion",1
            }, {
                "tracks",QJsonArray{
                }
            }
        };
    }
    bool MotionDocument::validate(const QJsonObject& d, QString* error) {
        auto fail=[error](const QString& why){
            if(error)*error=why;
            return false;
        };
        if(QJsonDocument(d).toJson(QJsonDocument::Compact).size()>262144) return fail(QStringLiteral("Motion document exceeds 256 KiB."));
        if(d.value("schemaVersion")!=QJsonValue(1) || !d.value("tracks").isArray()) return fail(QStringLiteral("Motion requires schemaVersion 1 and a tracks array."));
        const auto ts=d.value("tracks").toArray();
        if(ts.size()>32) return fail(QStringLiteral("At most 32 tracks are accepted."));
        for(const auto& v:ts) {
            if(!v.isObject()) return fail(QStringLiteral("Each track must be an object."));
            const auto t=v.toObject();
            for(const auto* k:{
                "name","state","property","easing"
            }
            ) if(!t.value(QLatin1String(k)).isString() || t.value(QLatin1String(k)).toString().isEmpty() || t.value(QLatin1String(k)).toString().size()>128) return fail(QStringLiteral("Track identity and easing require bounded nonempty strings."));
            for(const auto* k:{
                "enabled","mute","solo"
            }
            ) if(!t.value(QLatin1String(k)).isBool()) return fail(QStringLiteral("Track switches must be booleans."));
            if(!number(t,"duration",1,60000) || !number(t,"delay",-60000,60000)) return fail(QStringLiteral("Duration must be 1..60000 ms and delay -60000..60000 ms."));
            if(!t.value("keyframes").isArray()) return fail(QStringLiteral("Keyframes must be an array."));
            const auto fs=t.value("keyframes").toArray();
            if(fs.size()<2 || fs.size()>128) return fail(QStringLiteral("Each track requires 2..128 keyframes."));
            double previous=-1;
            const bool supported=properties().contains(t.value("property").toString());
            const auto range=bounds(t.value("property").toString());
            for(const auto& f:fs) {
                if(!f.isObject()) return fail(QStringLiteral("Each keyframe must be an object."));
                const auto o=f.toObject();
                if(!number(o,"offset",0,1) || o.value("offset").toDouble()<=previous) return fail(QStringLiteral("Offsets must strictly increase from 0 to 1."));
                previous=o.value("offset").toDouble();
                if(supported && !number(o,"value",range.first,range.second)) return fail(QStringLiteral("Keyframe value exceeds the renderer property bounds."));
            }
            if(fs.first().toObject().value("offset").toDouble()!=0 || fs.last().toObject().value("offset").toDouble()!=1) return fail(QStringLiteral("Tracks require explicit initial and final keyframes."));
        }
        return true;
    }
    int MotionDocument::duration(const QJsonObject& d) {
        if(!validate(d))return 0;
        int end=0;
        for(const auto& v:d.value("tracks").toArray()) {
            auto t=v.toObject();
            end=qMax(end,int(std::ceil(t.value("duration").toDouble()+t.value("delay").toDouble())));
        }
        return qBound(0,end,120000);
    }
    QJsonObject MotionDocument::sample(const QJsonObject& d,double ms,const QString& state,bool reduced) {
        QJsonObject result;
        if(!std::isfinite(ms)||!validate(d))return result;
        const auto ts=d.value("tracks").toArray();
        const auto renderable = [&state](const QJsonObject& track) {
            return properties().contains(track.value("property").toString())
                   && curves().contains(track.value("easing").toString())
                   && track.value("state").toString() == state
                   && track.value("enabled").toBool() && !track.value("mute").toBool();
        };
        bool solo=false;
        for(const auto& v:ts){
            auto t=v.toObject();
            if(renderable(t) && t.value("solo").toBool())solo=true;
        }
        for(const auto& v:ts) {
            const auto t=v.toObject();
            const QString p=t.value("property").toString();
            if(!renderable(t)||(solo&&!t.value("solo").toBool()))continue;
            const double x=reduced?1.0:qBound(0.0,(ms-t.value("delay").toDouble())/t.value("duration").toDouble(),1.0);
            const auto fs=t.value("keyframes").toArray();
            double value=fs.last().toObject().value("value").toDouble();
            for(int i=1;i<fs.size();++i) {
                auto a=fs.at(i-1).toObject(),b=fs.at(i).toObject();
                if(x<=b.value("offset").toDouble()) {
                    double u=(x-a.value("offset").toDouble())/(b.value("offset").toDouble()-a.value("offset").toDouble());
                    QEasingCurve c(t.value("easing").toString()==QLatin1String("inOutCubic")?QEasingCurve::InOutCubic:t.value("easing").toString()==QLatin1String("outCubic")?QEasingCurve::OutCubic:QEasingCurve::Linear);
                    value=a.value("value").toDouble()+(b.value("value").toDouble()-a.value("value").toDouble())*c.valueForProgress(u);
                    break;
                }
            }
            result.insert(p,qRound(value));
            // Ordered replace composition; geometry is pixel-aligned.
        }
        return result;
    }
    MotionPlayer::MotionPlayer(QObject* parent):QObject(parent) {
        m_timer.setInterval(16);
        m_timer.setTimerType(Qt::PreciseTimer);
        connect(&m_timer,&QTimer::timeout,this,[this]{
            seek(m_origin+m_direction*int(m_clock.elapsed()));if(m_position==0||m_position==MotionDocument::duration(m_document))pause();
        }
        );
    }
    MotionPlayer::~MotionPlayer(){
        pause();
    }
    bool MotionPlayer::setDocument(const QJsonObject& d,const QString& state) {
        if(!MotionDocument::validate(d))return false;
        pause();
        m_document=d;
        m_state=state;
        m_position=0;
        render();
        return true;
    }
    void MotionPlayer::render(){
        emit frame(MotionDocument::sample(m_document,m_position,m_state,m_reduced));
        emit positionChanged(m_position);
    }
    void MotionPlayer::seek(int ms){
        m_position=qBound(0,ms,MotionDocument::duration(m_document));
        render();
    }
    void MotionPlayer::play(bool reverse){
        pause();
        m_direction=reverse?-1:1;
        if(m_reduced){
            seek(MotionDocument::duration(m_document));
            return;
        }
        if(!reverse&&m_position>=MotionDocument::duration(m_document))seek(0);
        if(reverse&&m_position==0)seek(MotionDocument::duration(m_document));
        if(activePlayers.size()>=16)return;
        activePlayers.insert(this);
        m_origin=m_position;
        m_clock.restart();
        m_timer.start();
    }
    void MotionPlayer::pause(){
        m_timer.stop();
        activePlayers.remove(this);
    }
    void MotionPlayer::stop(){
        pause();
        m_position=0;
        emit frame({
        }
        );
        emit positionChanged(0);
    }
    void MotionPlayer::setReducedMotion(bool reduced){
        m_reduced=reduced;
        if(reduced)pause();
        render();
    }
    MotionEditor::MotionEditor(QWidget* parent):QWidget(parent) {
        setObjectName(QStringLiteral("appearanceMotionEditor"));
        auto* layout=new QVBoxLayout(this);
        auto* scope=new QLabel(tr("Target-scoped scalar motion. Normal-state tracks run when the target is shown. Other states and properties are retained without a live preview."),this);
        scope->setWordWrap(true);
        layout->addWidget(scope);
        m_tracks=new QTableWidget(0,9,this);
        m_tracks->setObjectName(QStringLiteral("motionTracks"));
        m_tracks->setAccessibleName(tr("Animation tracks, milliseconds and explicit property values"));
        m_tracks->setHorizontalHeaderLabels({
            tr("Name"),tr("State"),tr("Property"),tr("Duration ms"),tr("Delay ms"),tr("Easing"),tr("Enabled"),tr("Mute"),tr("Solo")
        }
        );
        m_tracks->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_tracks->setSelectionMode(QAbstractItemView::SingleSelection);
        m_tracks->setMinimumHeight(150);
        layout->addWidget(m_tracks);
        auto* actions=new QHBoxLayout;
        layout->addLayout(actions);
        auto button=[this,actions](const QString& text,auto fn){
            auto* b=new QPushButton(text,this);
            b->setAccessibleName(text);
            actions->addWidget(b);
            connect(b,&QPushButton::clicked,this,fn);
        };
        button(tr("Add track"),[this]{
            auto ts=m_document.value("tracks").toArray();if(ts.size()>=32)return;ts.append(track());auto d=m_document;d["tracks"]=ts;replaceDocument(d);m_tracks->selectRow(ts.size()-1);
        }
        );
        button(tr("Duplicate"),[this]{
            int r=m_tracks->currentRow();auto ts=m_document.value("tracks").toArray();if(r<0||ts.size()>=32)return;ts.insert(r+1,ts.at(r));auto d=m_document;d["tracks"]=ts;replaceDocument(d);
        }
        );
        button(tr("Remove"),[this]{
            int r=m_tracks->currentRow();auto ts=m_document.value("tracks").toArray();if(r<0)return;ts.removeAt(r);auto d=m_document;d["tracks"]=ts;replaceDocument(d);
        }
        );
        button(tr("Move up"),[this]{
            int r=m_tracks->currentRow();auto ts=m_document.value("tracks").toArray();if(r<1)return;auto t=ts.takeAt(r);ts.insert(r-1,t);auto d=m_document;d["tracks"]=ts;replaceDocument(d);m_tracks->selectRow(r-1);
        }
        );
        m_frames=new QTableWidget(0,2,this);
        m_frames->setObjectName(QStringLiteral("motionKeyframes"));
        m_frames->setAccessibleName(tr("Keyframe offsets from zero to one and property values"));
        m_frames->setHorizontalHeaderLabels({
            tr("Offset 0..1"),tr("Value")
        }
        );
        m_frames->setMinimumHeight(120);
        layout->addWidget(m_frames);
        auto* frameActions=new QHBoxLayout;
        layout->addLayout(frameActions);
        auto* add=new QPushButton(tr("Add midpoint"),this);
        frameActions->addWidget(add);
        connect(add,&QPushButton::clicked,this,[this]{
            int r=m_tracks->currentRow();auto ts=m_document.value("tracks").toArray();if(r<0)return;auto t=ts.at(r).toObject();auto fs=t.value("keyframes").toArray();if(fs.size()>=128)return;const int i=qBound(1,m_frames->currentRow()+1,fs.size()-1);auto a=fs.at(i-1).toObject(),b=fs.at(i).toObject();fs.insert(i,QJsonObject{
                {
                    "offset",(a.value("offset").toDouble()+b.value("offset").toDouble())/2
                }, {
                    "value",(a.value("value").toDouble()+b.value("value").toDouble())/2
                }
            }
            );t["keyframes"]=fs;ts[r]=t;auto d=m_document;d["tracks"]=ts;replaceDocument(d);
        }
        );
        auto* remove=new QPushButton(tr("Remove keyframe"),this);
        frameActions->addWidget(remove);
        connect(remove,&QPushButton::clicked,this,[this]{
            int r=m_tracks->currentRow(),f=m_frames->currentRow();auto ts=m_document.value("tracks").toArray();if(r<0)return;auto t=ts.at(r).toObject();auto fs=t.value("keyframes").toArray();if(f<=0||f>=fs.size()-1)return;fs.removeAt(f);t["keyframes"]=fs;ts[r]=t;auto d=m_document;d["tracks"]=ts;replaceDocument(d);
        }
        );
        m_preview=new QLabel(tr("Motion preview"),this);
        m_preview->setObjectName(QStringLiteral("motionPreviewSample"));
        m_preview->setMinimumHeight(40);
        layout->addWidget(m_preview);
        auto* playback=new QHBoxLayout;
        layout->addLayout(playback);
        m_seek=new QSpinBox(this);
        m_seek->setAccessibleName(tr("Timeline position in milliseconds"));
        playback->addWidget(m_seek);
        m_player=new MotionPlayer(this);
        m_player->setReducedMotion(MotionPolicy::instance()->reducedMotion());
        connect(MotionPolicy::instance(),&MotionPolicy::changed,this,[this]{
            m_player->setReducedMotion(MotionPolicy::instance()->reducedMotion());
        }
        );
        connect(m_seek,qOverload<int>(&QSpinBox::valueChanged),this,[this](int v){
            if(v!=m_player->position()){
                m_player->pause();m_player->seek(v);
            }
        }
        );
        connect(m_player,&MotionPlayer::positionChanged,m_seek,&QSpinBox::setValue);
        connect(m_player,&MotionPlayer::frame,this,[this](const QJsonObject& v){
            QFont f=font();if(v.contains("fontSize"))f.setPointSize(v.value("fontSize").toInt());m_preview->setFont(f);m_preview->setMinimumHeight(v.value("height").toInt(40));m_preview->setStyleSheet(QStringLiteral("background:#d5e7f5;color:#17212b;border:%1px solid #31536b;border-radius:%2px;padding:%3px;").arg(v.value("borderWidth").toInt()).arg(v.value("radius").toInt()).arg(v.value("spacing").toInt()));
        }
        );
        auto playButton=[this,playback](const QString& text,auto fn){
            auto* b=new QPushButton(text,this);
            playback->addWidget(b);
            connect(b,&QPushButton::clicked,this,fn);
        };
        playButton(tr("Play"),[this]{
            m_player->play();
        }
        );
        playButton(tr("Pause"),[this]{
            m_player->pause();
        }
        );
        playButton(tr("Stop"),[this]{
            m_player->stop();
        }
        );
        playButton(tr("Reverse"),[this]{
            m_player->play(true);
        }
        );
        auto* undo=new QPushButton(tr("Undo motion edit"),this);
        layout->addWidget(undo);
        connect(undo,&QPushButton::clicked,this,[this]{
            if(m_undo.isEmpty())return;m_redo.append(m_document);replaceDocument(m_undo.takeLast(),false);
        }
        );
        auto* redo=new QPushButton(tr("Redo motion edit"),this);
        layout->addWidget(redo);
        connect(redo,&QPushButton::clicked,this,[this]{
            if(m_redo.isEmpty())return;m_undo.append(m_document);replaceDocument(m_redo.takeLast(),false);
        }
        );
        auto* reset=new QPushButton(tr("Reset motion"),this);
        layout->addWidget(reset);
        connect(reset,&QPushButton::clicked,this,[this]{
            replaceDocument(MotionDocument::empty());
        }
        );
        m_status=new QLabel(this);
        m_status->setWordWrap(true);
        m_status->setAccessibleName(tr("Motion validation and capability status"));
        layout->addWidget(m_status);
        auto* unavailable=new QLabel(tr("Incomplete: layered targets, state triggers beyond show, transitions, grouping, markers, paths, colors, transforms, repeat/fill/alternate, custom curves, segment easing, spring physics, scroll/view timelines, multi-selection, clipboard keyframes, scheduled settings and durable append-only history. Qt QWidget has no native CSS custom-property or scroll/view timeline renderer; portable inputs remain preserved. Other listed capabilities require implementation."),this);
        unavailable->setWordWrap(true);
        layout->addWidget(unavailable);
        // Preserve the source copy separately so language changes never rewrite authored data.
        for(auto* widget:findChildren<QWidget*>()) {
            if(auto* label=qobject_cast<QLabel*>(widget); label && label!=m_status) widget->setProperty("motionSourceText",label->text());
            if(auto* button=qobject_cast<QAbstractButton*>(widget)) widget->setProperty("motionSourceText",button->text());
            if(!widget->accessibleName().isEmpty()) widget->setProperty("motionSourceAccessible",widget->accessibleName());
        }
        for(auto* table:{
            m_tracks,m_frames
        }
        ) {
            QStringList headers;
            for(int i=0;i<table->columnCount();++i)headers.append(table->horizontalHeaderItem(i)->text());
            table->setProperty("motionSourceHeaders",headers);
        }
        auto refreshCopy=[this] {
            if(m_status->property("motionStatusSource").isValid())setStatus(m_status->property("motionStatusSource").toString());
            for(auto* widget:findChildren<QWidget*>()) {
                const auto text=widget->property("motionSourceText");
                if(text.isValid()) {
                    if(auto* label=qobject_cast<QLabel*>(widget))label->setText(motionCopy(text.toString()));
                    if(auto* button=qobject_cast<QAbstractButton*>(widget))button->setText(motionCopy(text.toString()));
                }
                const auto name=widget->property("motionSourceAccessible");
                if(name.isValid())widget->setAccessibleName(motionCopy(name.toString()));
            }
            for(auto* table:{
                m_tracks,m_frames
            }
            ) {
                QStringList headers;
                for(const auto& source:table->property("motionSourceHeaders").toStringList())headers.append(motionCopy(source));
                table->setHorizontalHeaderLabels(headers);
            }
        };
        connect(Voice::notifier(),&Voice::Notifier::changed,this,refreshCopy);
        refreshCopy();
        connect(m_tracks,&QTableWidget::itemSelectionChanged,this,[this]{
            if(!m_loading)showFrames();
        }
        );
        connect(m_tracks,&QTableWidget::cellChanged,this,[this]{
            saveTables();
        }
        );
        connect(m_frames,&QTableWidget::cellChanged,this,[this]{
            saveTables();
        }
        );
    }
    void MotionEditor::setStatus(const QString& source){
        m_status->setProperty("motionStatusSource",source);
        m_status->setText(motionCopy(source).arg(m_key));
    }
    void MotionEditor::hideEvent(QHideEvent* event){
        stopPreview();
        QWidget::hideEvent(event);
    }
    void MotionEditor::stopPreview(){
        m_player->stop();
    }
    void MotionEditor::setTarget(const QString& key){
        stopPreview();
        m_key=key;
        m_undo.clear();
        m_redo.clear();
        m_document=ElementOverrides::instance()->get(key).motion;
        if(m_document.isEmpty())m_document=MotionDocument::empty();
        refresh();
    }
    void MotionEditor::replaceDocument(const QJsonObject& d,bool history){
        QString error;
        if(!MotionDocument::validate(d,&error)){
            setStatus(error);
            return;
        }
        if(m_key.isEmpty())return;
        if(history){
            m_undo.append(m_document);
            if(m_undo.size()>100)m_undo.removeFirst();
            m_redo.clear();
        }
        m_document=d;
        auto v=ElementOverrides::instance()->get(m_key);
        v.motion=d;
        ElementOverrides::instance()->set(m_key,v);
        refresh();
    }
    void MotionEditor::refresh(){
        QString diagnostic;
        if(!MotionDocument::validate(m_document,&diagnostic)) {
            m_loading=true;m_tracks->setRowCount(0);m_frames->setRowCount(0);m_loading=false;
            m_player->setDocument(MotionDocument::empty());m_seek->setRange(0,0);setStatus(diagnostic);return;
        }

        m_loading=true;
        int selected=m_tracks->currentRow();
        auto ts=m_document.value("tracks").toArray();
        m_tracks->setRowCount(ts.size());
        const QStringList keys{
            "name","state","property","duration","delay","easing","enabled","mute","solo"
        };
        for(int r=0;r<ts.size();++r){
            auto t=ts.at(r).toObject();
            for(int c=0;c<keys.size();++c){
                auto v=t.value(keys[c]);
                m_tracks->setItem(r,c,new QTableWidgetItem(v.isBool()?(v.toBool()?QStringLiteral("true"):QStringLiteral("false")):v.isDouble()?QString::number(v.toDouble(),'g',16):v.toString()));
            }
        }
        m_loading=false;
        if(!ts.isEmpty())m_tracks->selectRow(qBound(0,selected,ts.size()-1));
        showFrames();
        m_seek->setRange(0,MotionDocument::duration(m_document));
        m_player->setDocument(m_document);
        QString invalid;
        if (!MotionDocument::validate(m_document,&invalid)) {
            setStatus(invalid);
            return;
        }
        setStatus(QStringLiteral("Saved for %1. Normal state; ordered replace, explicit endpoints, millisecond clock, fill before and after. Unknown fields are retained; unavailable values are not rendered."));
    }
    void MotionEditor::showFrames(){
        m_loading=true;
        int r=m_tracks->currentRow();
        auto ts=m_document.value("tracks").toArray();
        auto fs=r>=0&&r<ts.size()?ts.at(r).toObject().value("keyframes").toArray():QJsonArray{
        };
        m_frames->setRowCount(fs.size());
        for(int i=0;i<fs.size();++i){
            auto f=fs.at(i).toObject();
            m_frames->setItem(i,0,new QTableWidgetItem(QString::number(f.value("offset").toDouble(),'g',16)));
            m_frames->setItem(i,1,new QTableWidgetItem(f.value("value").isDouble()?QString::number(f.value("value").toDouble(),'g',16):QString::fromUtf8(QJsonDocument(QJsonArray{
                f.value("value")
            }
            ).toJson(QJsonDocument::Compact))));
        }
        m_loading=false;
    }
    void MotionEditor::saveTables(){
        if(m_loading)return;
        auto ts=m_document.value("tracks").toArray();
        const QStringList keys{
            "name","state","property","duration","delay","easing","enabled","mute","solo"
        };
        for(int r=0;r<m_tracks->rowCount();++r){
            auto t=ts.at(r).toObject();
            for(int c=0;c<9;++c){
                auto* item=m_tracks->item(r,c);
                if(!item)return;
                const auto text=item->text();
                if(c==3||c==4){
                    bool ok=false;
                    double n=text.toDouble(&ok);
                    if(!ok){
                        setStatus(QStringLiteral("Timing must be numeric."));
                        return;
                    }
                    t[keys[c]]=n;
                }
                else if(c>=6){
                    if(text!=QLatin1String("true")&&text!=QLatin1String("false")){
                        setStatus(QStringLiteral("Switches require true or false."));
                        return;
                    }
                    t[keys[c]]=text==QLatin1String("true");
                }
                else t[keys[c]]=text;
            }
            ts[r]=t;
        }
        int r=m_tracks->currentRow();
        if(r>=0){
            auto t=ts.at(r).toObject();
            if(MotionDocument::properties().contains(t.value("property").toString())){
                auto fs=t.value("keyframes").toArray();
                for(int i=0;i<m_frames->rowCount();++i){
                    auto f=fs.at(i).toObject();
                    for(int c=0;c<2;++c){
                        auto* item=m_frames->item(i,c);
                        if(!item)return;
                        bool ok=false;
                        double n=item->text().toDouble(&ok);
                        if(!ok){
                            setStatus(QStringLiteral("Keyframes must be numeric."));
                            return;
                        }
                        f[c==0?QStringLiteral("offset"):QStringLiteral("value")]=n;
                    }
                    fs[i]=f;
                }
                t["keyframes"]=fs;
            }
            ts[r]=t;
        }
        auto d=m_document;
        d["tracks"]=ts;
        replaceDocument(d);
    }
}
