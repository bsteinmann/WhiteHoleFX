// update_checker.cpp: Umsetzung zu update_checker.hpp.
#include "update_checker.hpp"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>

#include <cstdlib>

namespace whitehole::studio {

namespace {

// Neuestes veröffentlichtes Release (GitHub lässt Entwürfe und Vorabversionen hier weg).
const char* const kLatestReleaseUrl = "https://api.github.com/repos/bsteinmann/WhiteHoleFX/releases/latest";
const char* const kConsentKey = "update/autoCheck";
constexpr int kTimeoutMs = 10000;
constexpr qint64 kMaxAnswerBytes = 1 << 20;  // 1 MiB reicht weit; mehr ist keine Antwort, die wir lesen wollen

}  // namespace

UpdateConsent updateConsent() {
    QSettings settings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")};
    if (!settings.contains(QLatin1String{kConsentKey})) {
        return UpdateConsent::Unasked;
    }
    return settings.value(QLatin1String{kConsentKey}).toBool() ? UpdateConsent::Yes : UpdateConsent::No;
}

void setUpdateConsent(UpdateConsent consent) {
    QSettings settings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")};
    if (consent == UpdateConsent::Unasked) {
        settings.remove(QLatin1String{kConsentKey});
    } else {
        settings.setValue(QLatin1String{kConsentKey}, consent == UpdateConsent::Yes);
    }
}

bool updateCheckDisabledByEnvironment() {
    const char* value = std::getenv("WHITEHOLEFX_NO_UPDATE_CHECK");
    return value != nullptr && value[0] != '\0' && value[0] != '0';
}

UpdateChecker::UpdateChecker(QObject* parent) : QObject{parent} {
    qRegisterMetaType<UpdateChecker::Result>();
    network_.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);  // nie von https auf http
}

core::Version UpdateChecker::currentVersion() {
    return core::parseVersion(WH_VERSION).value_or(core::Version{});
}

void UpdateChecker::start() {
    if (reply_ != nullptr) {
        return;
    }
    QNetworkRequest request{QUrl{QString::fromLatin1(kLatestReleaseUrl)}};
    request.setRawHeader("User-Agent", QByteArray{"WhiteHoleFX/"} + WH_VERSION);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(kTimeoutMs);
    reply_ = network_.get(request);
    reply_->setReadBufferSize(kMaxAnswerBytes);
    connect(reply_, &QNetworkReply::finished, this, &UpdateChecker::onReply);
}

void UpdateChecker::onReply() {
    QNetworkReply* reply = reply_;
    reply_ = nullptr;
    reply->deleteLater();
    Result result;
    if (reply->error() != QNetworkReply::NoError) {
        result.error = reply->errorString();
    } else {
        result = evaluate(reply->read(kMaxAnswerBytes), currentVersion());
    }
    Q_EMIT finished(result);
}

UpdateChecker::Result UpdateChecker::evaluate(const QByteArray& json, const core::Version& current) {
    Result result;
    const auto release = core::parseLatestRelease(std::string_view{json.constData(), static_cast<std::size_t>(json.size())});
    if (!release) {
        result.error = tr("Die Antwort von GitHub war nicht lesbar.");
        return result;
    }
    if (release->version > current) {
        result.outcome = Outcome::Available;
        result.version = QString::fromStdString(core::toString(release->version));
        result.url = QString::fromStdString(release->url);
    } else {
        result.outcome = Outcome::UpToDate;
        result.version = QString::fromStdString(core::toString(release->version));
    }
    return result;
}

}  // namespace whitehole::studio
