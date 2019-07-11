#ifndef _MYTHSESSION_H_
#define _MYTHSESSION_H_

#include "mythbaseexp.h"

#include <QString>
#include <QDateTime>
#include <QMap>

enum DigestUserActions {
    DIGEST_USER_ADD,
    DIGEST_USER_REMOVE,
    DIGEST_USER_CHANGE_PW
};

class MBASE_PUBLIC MythUserSession
{
  public :
    MythUserSession() = default;
   ~MythUserSession() { m_sessionToken.fill('0'); m_sessionToken.clear(); }

    /**
     * \brief Check if this session object appears properly constructed, it
     *        DOES NOT validate whether it is a valid session with the
     *        SessionManager
     *
     * A valid session object includes a user id, a session token of the correct
     * length, a created date in the past and an expiry date in the future.
     *
     * It may also require that the 'client' string is present and formatted as
     * expected.
     */
    bool IsValid(void) const;

    const QString GetUserName(void) const { return m_name; }
    uint  GetUserId(void) const { return m_userId; }

    const QString GetSessionToken(void) const { return m_sessionToken; }
    const QString GetSessionClient(void) const { return m_sessionClient; }

    const QDateTime GetSessionCreated() const { return m_sessionCreated; }
    const QDateTime GetSessionLastActive() const { return m_sessionLastActive; }
    const QDateTime GetSessionExpires() const { return m_sessionExpires; }

    /**
     * \brief Check if the user has the given permission in a context
     *
     * This function is currently unimplemented.
     *
     * \param context
     * \param permission
     */
    bool CheckPermission( const QString &context, uint permission);

  protected:
    /**
     * \brief Save the session to the database
     *
     * This is intended only to be used by the SessionManager, everything except
     * for checking perms is done in the session manager, and perms checking is
     * only done through the MythUserSession object to
     */
    bool Save(void);

    /**
     * \brief Update session expiry and access times
     */
    bool Update(void);

    uint    m_userId {0};
    QString m_name;

    QString m_sessionToken;
    QDateTime m_sessionCreated;
    QDateTime m_sessionLastActive; // For informational purposes
    QDateTime m_sessionExpires;
    QString m_sessionClient;

    QMap<QString, uint> m_permissionsList; // Context, flags

  friend class MythSessionManager;
};

/**
 * \class MythSessionManager
 *
 * We use digest authentication because it protects the password over
 * unprotected networks. Even if traffic between the client and server is
 * captured, the digest and password cannot be determined and the
 * attacker cannot gain system access in that way. It cannot protect against
 * a full man-in-the-middle but if that really is a concern, users should
 * setup TLS.
 *
 * The digest isn't very strong if it leaked, but for that to happen the
 * database would need to be breached, at which point the MythTV system is
 * already heavily compromised.
 */
class MBASE_PUBLIC MythSessionManager
{
  public :
    MythSessionManager();
   ~MythSessionManager() = default;

    /**
     * \brief Check if the given user exists but not whether there is a valid
     *        session open for them!
     * \param username
     */
    bool IsValidUser(const QString &username);

    /**
     * \brief Check if the session token is valid
     * \param sessionToken
     */
    bool IsValidSession(const QString &sessionToken);

    /**
     * \brief Load the session details and return
     * \param sessionToken
     *
     * If no matching session exists an empty MythUserSession object is returned
     */
    MythUserSession GetSession(const QString &sessionToken);

    /**
     * \brief Load the password digest for comparison in the HTTP Auth code
     * \param username
     *
     * The username should be checked for validity first
     */
    QString GetPasswordDigest(const QString &username);

    /**
     * \brief Login user by digest
     * \param username
     * \param digest Password Digest (RFC 2617)
     * \param client Optional string identifying this client uniquely,
     *               will be created automatically if absent
     * \return Unique session token
     */
    MythUserSession LoginUser(const QString &username,
                              const QByteArray &digest,
                              const QString &client = "");

    /**
     * \brief Login user by password - convenient alternative to using the digest
     * \param username
     * \param password Clear text password string
     * \param client Optional string identifying this client uniquely,
     *               will be created automatically if absent
     * \return Unique session token
     */
    MythUserSession LoginUser(const QString &username,
                              const QString &password,
                              const QString &client = "");

    /**
     * \brief Generate a digest string
     * \param username
     * \param password
     */
    static QByteArray CreateDigest(const QString &username,
                                   const QString &password);

    /**
     * \brief Manage digest user entries
     * \param action requires one from: DigestUserActions
     * \param username always required
     * \param password always required
     * \param newPassword if action=DIGEST_USER_CHANGE_PW
     * \param adminPassword if action=DIGEST_USER_ADD
     */
    bool ManageDigestUser(DigestUserActions action,
                          const QString    &username,
                          const QString    &password,
                          const QString    &newPassword,
                          const QString    &adminPassword);

  private:
    /**
     * \brief Load the values from the sessions table on startup
     */
    void LoadSessions(void);

    /**
     * \brief Update the session timestamps
     */
    void UpdateSession(const QString &sessionToken);

    /**
     * \brief Checks if an existing session for this user and client exists
     * \param username
     * \param client
     *
     * We may want to reuse this session or destroy this session
     */
    MythUserSession GetSession(const QString &username, const QString &client);

    /**
     * \brief Add new user session to the database and cache
     */
    MythUserSession CreateUserSession(uint userId, const QString &username,
                                      const QString &client);

    /**
     * \brief Removes user session from the database and cache
     */
    void DestroyUserSession(const QString &sessionToken);

    bool AddDigestUser(const QString &username,
                       const QString &password,
                       const QString &adminPassword);

    bool RemoveDigestUser(const QString &username,
                          const QString &password);

    bool ChangeDigestUserPassword(const QString &username,
                                  const QString &oldPassword,
                                  const QString &newPassword);

    QMap<QString, MythUserSession> m_sessionList;
};

#endif
