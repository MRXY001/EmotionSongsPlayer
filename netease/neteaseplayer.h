#ifndef NETEASEPLAYER_H
#define NETEASEPLAYER_H

#include <QObject>
#include <QMediaPlayer>
#include <QTimer>
#include <QSettings>
#include "neteasegetter.h"

#define PREPAIR_NEXT_SONG_AHEAD 30000

class NeteasePlayer : public QObject
{
    Q_OBJECT
public:
    NeteasePlayer(QObject *parent = nullptr);

    void setDataDir(QString path);
    NeteaseGetter* getGetter();

    bool isPlaying();

signals:

public slots:
    void randomPlay(QString type);
    void setType(QString type);
    void play();
    void stop();
    void next();
    void block();
    QString information();

private:
    void playerPlay(QString id);

private:
    NeteaseGetter* musics;
    QMediaPlayer* player;

    QString current_type; // 当前类型，如果和 music 的 type 不一样，则重新获取（提前）
    QString data_dir;

    bool _flag_nexting; // 是否正在下一个（设置媒体会触发 State=Stop，从而误触播放完毕事件）
    bool _flag_prepaired_next; // 是否已经准备下一个了
    qint64 _prepair_duration; // 播放到这个时间就提前准备下一个
};

#endif // NETEASEPLAYER_H
