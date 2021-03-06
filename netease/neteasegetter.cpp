#include "neteasegetter.h"

NeteaseGetter::NeteaseGetter(QObject *parent) : QObject(parent), data_dir("musics/"), settings(nullptr), use_fixed(false), use_type_fixed(false)
{
    srand(time(0));
}

void NeteaseGetter::setDataDir(QString path)
{
    data_dir = path;

    if (settings)
        delete settings;
    settings = new QSettings("netease");
}

QString NeteaseGetter::getType()
{
    return this->type;
}

void NeteaseGetter::save()
{
    if (!settings)
        return ;

    settings->setValue("fixed/use_fixed", use_fixed);
    settings->setValue("fixed/use_type_fixed", use_type_fixed);
    settings->setValue("fixed/songList", fixed_songList);
    settings->setValue("fixed/type_songList_keys", QStringList(type_fixed_songList_map.keys()));
    for (auto it = type_fixed_songList_map.begin(); it != type_fixed_songList_map.end(); it++)
    {
        settings->setValue("fixed_type/" + it.key(), it.value());
    }
    settings->setValue("format/search_format", search_format);
    settings->setValue("format/black_list", black_list);
}

void NeteaseGetter::restore()
{
    if (!settings)
        return ;

    use_fixed = settings->value("fixed/use_fixed", use_fixed).toBool();
    use_type_fixed = settings->value("fixed/use_type_fixed", use_type_fixed).toBool();
    fixed_songList = settings->value("fixed/songList", fixed_songList).toStringList();
    QStringList keys = settings->value("fixed/type_songList_keys", QStringList(type_fixed_songList_map.keys())).toStringList();
    type_fixed_songList_map.clear();
    foreach (QString key, keys)
    {
        type_fixed_songList_map.insert(key, settings->value("fixed_type/"+key, QStringList()).toStringList());
    }
    search_format = settings->value("format/search_format", search_format).toString();
    black_list = settings->value("format/black_list", black_list).toStringList();
}

void NeteaseGetter::setUseFixed(bool enable)
{
    this->use_fixed = enable;
    save();
}

void NeteaseGetter::setUseTypeFixed(bool enable)
{
    use_type_fixed = enable;
    save();
}

void NeteaseGetter::addFixedSongList(QString id)
{
    fixed_songList.append(id);
    save();
}

void NeteaseGetter::removeFixedSongList(QString id)
{
    fixed_songList.removeOne(id);
    save();
}

void NeteaseGetter::addTypeFixedSongList(QString type, QString id)
{
    type_fixed_songList_map[type].append(id);
    save();
}

void NeteaseGetter::removeTypeFixedSongList(QString type, QString id)
{
    type_fixed_songList_map[type].removeOne(id);
    save();
}

void NeteaseGetter::setSearchFormat(QString format)
{
    if (format.isEmpty())
        search_format = "%1";
    this->search_format = format;
    save();
}

void NeteaseGetter::addBlackList(QString b)
{
    black_list.append(b);
}

void NeteaseGetter::removeBlackList(QString b)
{
    black_list.removeOne(b);
    save();
}

void NeteaseGetter::searchNetListByType(QString type)
{
    NETEASE_DEB "开始搜索："+type;
    this->type = type;
    QString url = NeteaseAPI::getSongListSearchUrl(type);
    connect(new NetUtil(url), &NetUtil::finished, this, [=](QString result) {
        NETEASE_DEB "搜索结果" << QString::number(result.length());
        if (result.length() < 100)
            NETEASE_DEB result;
        songList_list = decodeSongListList(result);
        // 清空则取消非当前类型的播放列表
//        current_songList = SongList();
//        next_song = Song();

        if (songList_list.size() == 0)
        {
            qDebug() << "未搜索到分类";
            return ;
        }

        // 随机获取一个歌单（下一个准备播放的）
        getRandomSongListInResult();
    });
}

void NeteaseGetter::getRandomSongListInResult()
{
    if (songList_list.size() == 0)
        return ;
    int index = rand() % songList_list.size();
    current_songList = songList_list.at(index);
    NETEASE_DEB "下载随机歌单" << current_songList.name << current_songList.id;
    getNetList(current_songList.id);
}

void NeteaseGetter::getNetList(QString id)
{
    NETEASE_DEB "获取歌单："+id;
    QString url = NeteaseAPI::getSongListUrl(id);
    connect(new NetUtil(url), &NetUtil::finished, this, [=](QString result) {
        NETEASE_DEB "搜索结果" << QString::number(result.length());
        if (result.length() < 100)
            NETEASE_DEB result;
        current_songList = decodeSongList(result);

        // 如果这个歌单是空的，重新换一个下载
        if (current_songList.songs.size() == 0)
        {
            NETEASE_DEB "当前歌单没有歌曲，换一个";
            // 移除这个无效的
            for (int i = 0; i < songList_list.size(); i++)
            {
                if (songList_list.at(i).id == current_songList.id)
                {
                    songList_list.removeAt(i);
                    break;
                }
            }

            // 重新换一个
            getRandomSongListInResult();
            return ;
        }

        prepareNextSong(); // 可能是初次准备，也可能是提前准备下一首歌
    });
}

void NeteaseGetter::downloadNetSong(QString id)
{
    QString url = NeteaseAPI::getSongDownloadUrl(id);
    NETEASE_DEB "开始下载歌曲："+id << url;

    // 判断文件是否已存在
    if (isFileExist(data_dir + id + ".mp3") || id.isEmpty())
    {
        NETEASE_DEB "歌曲已下载";
        emit signalDownloadFinished(id);
        return ;
    }
    ensureDirExist(data_dir);

    connect(new NetUtil(url), &NetUtil::finished, this, [=](QString result) {
        QString durl = NetUtil::extractOne(result, "\"url\":\"(.+?)\"");
        NETEASE_DEB "歌曲地址" << durl;

        if (durl.isEmpty())
        {
            NETEASE_DEB "找不到下载地址";
            getNextSong(); // 下载失败，播放下一首
            return ;
        }
        NetUtil* net2 = new NetUtil;
        net2->download(durl, data_dir + id + ".mp3");
        connect(net2, &NetUtil::finished, this, [=](QString result) {
            NETEASE_DEB "下载完毕:" << result;
            emit signalDownloadFinished(id);
            net2->deleteLater();
        });
    });

    /*NetUtil* net = new NetUtil;
    net->getRedirection(url);
    connect(net, &NetUtil::redirected, this, [=](QString url) {
        NETEASE_DEB "真实网址：" << url;
        NetUtil* net2 = new NetUtil;
        net2->download(url, data_dir + id + ".mp3");
        connect(net2, &NetUtil::finished, this, [=](QString result) {
            NETEASE_DEB "下载完毕:" << result;
            emit signalDownloadFinished(id);
            net2->deleteLater();
        });
        net->deleteLater();
    });*/
}

/**
 * 准备下一首歌
 * 就是在当前歌曲快要放完的时候，提前下载，以免断开
 */
QString NeteaseGetter::prepareNextSong()
{
    if (current_songList.songs.size() == 0)
    {
        qDebug() << "没有歌单";
        return "";
    }

    if (current_songList.songs.size() == 0)
        return "";
    int index = rand() % current_songList.songs.size();
    next_song = current_songList.songs.at(index);
    NETEASE_DEB "下载随机歌曲" << next_song.name << next_song.id;
    downloadNetSong(next_song.id);
    return next_song.id;
}

/**
 * 获取下一首歌，即 next_song
 * 如果不存在，则返回空，并且下载新的
 */
QString NeteaseGetter::getNextSong()
{
    if (!next_song.id.isEmpty())
    {
        current_song = next_song;
        next_song = Song(); // 清空这一首
        return current_song.id;
    }

    // 不存在下一首歌，开始下载
    prepareNextSong();
    return "";
}

/**
 * 如果下载完毕，直接播放时
 * 判断是当前歌曲还是下一首歌
 */
void NeteaseGetter::isCurrentOrNext(QString id)
{
    if (current_song.id == id) // 正在播放当前歌曲，没事
        return ;
    else if (next_song.id == id) // 如果已经开始了下一首歌
    {
        current_song = next_song;
        next_song = Song();
    }
}

QList<SongList> NeteaseGetter::decodeSongListList(QString result)
{
    QList<SongList> songList_list;

    // 解析搜索结果
    // 因为是一开始的API尝试连接，所以报错内容尽量准确吧
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(result.toUtf8(), &error);
    if (doc.isNull() || error.error != QJsonParseError::NoError)
    {
        qDebug() << "搜索结果解析错误" << error.error << error.errorString();
        return songList_list;
    }
    if (!doc.isObject())
    {
        qDebug() << "搜索结果不是对象";
        return songList_list;
    }
    QJsonObject object = doc.object();
    if (!object.contains("result"))
    {
        qDebug() << "搜索结果没有 result";
        return songList_list;
    }
    QJsonObject data = object.value("result").toObject();
    if (!data.contains("playlists") || data.value("playlists").type() != QJsonValue::Array)
    {
        qDebug() << "playlists 出错";
        return songList_list;
    }
    QJsonArray playLists = data.value("playlists").toArray();
    foreach (QJsonValue value, playLists)
    {
        if (value.type() != QJsonValue::Object)
        {
            qDebug() << "歌单不是 Object 类型" << value.toString();
            continue;
        }
        QJsonObject object = value.toObject();
        songList_list.append(decodeSongList(value.toObject()));
    }

    return songList_list;
}

SongList NeteaseGetter::decodeSongList(QJsonObject object)
{
    SongList songList;

    songList.id = QString::number(object.value("id").toInt());
    songList.name = object.value("name").toString();
    songList.description = object.value("description").toString();
    songList.creator_nickname = object.value("creator").toObject().value("nickname").toString();

//    NETEASE_DEB songList.name << songList.id << songList.creator_nickname;

    return songList;
}

SongList NeteaseGetter::decodeSongList(QString result)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(result.toUtf8(), &error);
    SongList songList;
    if (doc.isNull() || error.error != QJsonParseError::NoError)
    {
        qDebug() << "搜索结果解析错误" << error.error << error.errorString();
        return songList;
    }
    if (!doc.isObject())
    {
        qDebug() << "搜索结果不是对象";
        return songList;
    }
    QJsonObject object = doc.object();
    if (!object.contains("playlist"))
    {
        qDebug() << "搜索结果没有 playlist";
        return songList;
    }
    QJsonObject data = object.value("playlist").toObject();

    songList.id = QString::number(object.value("id").toInt());
    songList.name = data.value("name").toString();
    songList.description = data.value("description").toString();
    songList.creator_nickname = data.value("creator").toObject().value("nickname").toString();
    QJsonArray tracks = data.value("tracks").toArray();
    foreach (QJsonValue value, tracks)
    {
        songList.songs.append(decodeSong(value.toObject()));
    }
    return songList;
}

Song NeteaseGetter::decodeSong(QJsonObject object)
{
    Song song;
    song.id = QString::number(object.value("id").toInt());
    song.name = object.value("name").toString();
    song.ar_name = object.value("ar").toObject().value("name").toString();

//    NETEASE_DEB song.name << song.id << song.ar_name;

    return song;
}
