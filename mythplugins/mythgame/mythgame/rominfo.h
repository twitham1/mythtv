#ifndef ROMINFO_H_
#define ROMINFO_H_

#include <QMetaType>
#include <QString>
#include <QList>

int romInDB(const QString& rom, const QString& gametype);

class RomInfo
{
  public:
    static QList<RomInfo*> GetAllRomInfo();
    static RomInfo *GetRomInfoById(int id);

    RomInfo(int lid = 0, QString lromname = "", QString lsystem = "", QString lgamename ="",
            QString lgenre = "", QString lyear = "", bool lfavorite = false,
            QString lrompath = "", QString lcountry ="", QString lcrc_value = "",
            int ldiskcount = 0, QString lgametype = "", int lromcount = 0,
            QString lallsystems = "", QString lplot = "", QString lpublisher = "",
            QString lversion = "", QString lscreenshot = "", QString lfanart = "",
            QString lboxart = "", QString linetref = "")
            {
                m_id = lid;
                m_romname = lromname;
                m_system = lsystem;
                m_gamename = lgamename;
                m_genre = lgenre;
                m_year = lyear;
                m_favorite = lfavorite;
                m_rompath = lrompath;
                m_screenshot = lscreenshot;
                m_fanart = lfanart;
                m_boxart = lboxart;
                m_country = lcountry;
                m_crc_value = lcrc_value;
                m_diskcount = ldiskcount;
                m_gametype = lgametype;
                m_romcount = lromcount;
                m_allsystems = lallsystems;
                m_plot = lplot;
                m_publisher = lpublisher;
                m_version = lversion;
                m_inetref = linetref;
            }

    RomInfo(const RomInfo &lhs)
            {
                m_id = lhs.m_id;
                m_romname = lhs.m_romname;
                m_system = lhs.m_system;
                m_gamename = lhs.m_gamename;
                m_genre = lhs.m_genre;
                m_year = lhs.m_year;
                m_favorite = lhs.m_favorite;
                m_rompath = lhs.m_rompath;
                m_screenshot = lhs.m_screenshot;
                m_fanart = lhs.m_fanart;
                m_boxart = lhs.m_boxart;
                m_country = lhs.m_country;
                m_crc_value = lhs.m_crc_value;
                m_diskcount = lhs.m_diskcount;
                m_gametype = lhs.m_gametype;
                m_romcount = lhs.m_romcount;
                m_allsystems = lhs.m_allsystems;
                m_plot = lhs.m_plot;
                m_publisher = lhs.m_publisher;
                m_version = lhs.m_version;
                m_inetref = lhs.m_inetref;
            }

    ~RomInfo() = default;

    bool FindImage(QString BaseFileName, QString *result);

    int Id() const { return m_id; }
    void setId(const int &lid) { m_id = lid; }

    QString Rompath() const { return m_rompath; }
    void setRompath(const QString &lrompath) { m_rompath = lrompath; }

    QString Screenshot() const { return m_screenshot; }
    void setScreenshot(const QString &lscreenshot) { m_screenshot = lscreenshot; }

    QString Fanart() const { return m_fanart; }
    void setFanart(const QString &lfanart) { m_fanart = lfanart; }

    QString Boxart() const { return m_boxart; }
    void setBoxart(const QString &lboxart) { m_boxart = lboxart; }

    QString Romname() const { return m_romname; }
    void setRomname(const QString &lromname) { m_romname = lromname; }

    QString System() const { return m_system; }
    void setSystem(const QString &lsystem) { m_system = lsystem; }

    QString Gamename() const { return m_gamename; }
    void setGamename(const QString &lgamename) { m_gamename = lgamename; }

    QString Genre() const { return m_genre; }
    void setGenre(const QString &lgenre) { m_genre = lgenre; }

    QString Country() const { return m_country; }
    void setCountry(const QString &lcountry) { m_country = lcountry; }

    QString GameType() const { return m_gametype; }
    void setGameType(const QString &lgametype) { m_gametype = lgametype; }

    int RomCount() const { return m_romcount; }
    void setRomCount(const int &lromcount) { m_romcount = lromcount; }

    QString AllSystems() const { return m_allsystems; }
    void setAllSystems(const QString &lallsystems) { m_allsystems = lallsystems; }

    int DiskCount() const { return m_diskcount; }
    void setDiskCount(const int &ldiskcount) { m_diskcount = ldiskcount; }

    QString CRC_VALUE() const { return m_crc_value; }
    void setCRC_VALUE(const QString &lcrc_value) { m_crc_value = lcrc_value; }

    QString Plot() const { return m_plot; }
    void setPlot(const QString &lplot) { m_plot = lplot; }

    QString Publisher() const { return m_publisher; }
    void setPublisher(const QString &lpublisher) { m_publisher = lpublisher; }

    QString Version() const { return m_version; }
    void setVersion(const QString &lversion) { m_version = lversion; }

    QString Year() const { return m_year; }
    void setYear(const QString &lyear) { m_year = lyear; }

    QString Inetref() const { return m_inetref; }
    void setInetref(const QString &linetref) { m_inetref = linetref; }

    int Favorite() const { return m_favorite; }
    void setFavorite(bool updateDatabase = false);

    QString getExtension();
    QString toString();

    void setField(const QString& field, const QString& data);
    void fillData();

    void SaveToDatabase();
    void DeleteFromDatabase();

  protected:
    int     m_id;
    QString m_romname;
    QString m_system;
    QString m_gamename;
    QString m_genre;
    QString m_country;
    QString m_crc_value;
    QString m_gametype;
    QString m_allsystems;
    QString m_plot;
    QString m_publisher;
    QString m_version;
    int     m_romcount;
    int     m_diskcount;
    QString m_year;
    bool    m_favorite;
    QString m_rompath;
    QString m_screenshot;
    QString m_fanart;
    QString m_boxart;
    QString m_inetref;
};

bool operator==(const RomInfo& a, const RomInfo& b);

Q_DECLARE_METATYPE(RomInfo *)

#endif
