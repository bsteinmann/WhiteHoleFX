// pong_sim.cpp: Umsetzung zu pong_sim.hpp. Aufschlag, Flug, Schlägertreffer, Punkte und Spielende.
#include "pong_sim.hpp"

#include <algorithm>
#include <cmath>

namespace whitehole::nodes::detail {

namespace {

constexpr double kStep = 1.0 / 240.0;
constexpr double kMaxFrame = 0.25;  // längere Pausen (Debugger, Ruhezustand) werden nicht nachgespielt
constexpr double kMaxSpeedUp = 3.0;  // Tempo wächst höchstens auf das Dreifache

double fold(double y, double lo, double hi) {
    // Spiegelt y zwischen lo und hi hin und her (Weg des Balls mit Wandabprallern).
    const double span = hi - lo;
    if (span <= 0.0) {
        return lo;
    }
    double t = std::fmod(y - lo, 2.0 * span);
    if (t < 0.0) {
        t += 2.0 * span;
    }
    return lo + (t <= span ? t : 2.0 * span - t);
}

double clampPaddle(double center, double size) {
    const double half = std::min(size, 1.0) * 0.5;
    return std::clamp(center, half, 1.0 - half);
}

}  // namespace

double PongSim::uniform() {
    // splitmix64
    rng_ += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = rng_;
    z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
    z ^= z >> 31U;
    return static_cast<double>(z >> 11U) / 9007199254740992.0;  // 2^53
}

PongView PongSim::view() const {
    PongView v;
    v.ballX = ballX_;
    v.ballY = ballY_;
    v.left = left_;
    v.right = right_;
    v.scoreLeft = scoreLeft_;
    v.scoreRight = scoreRight_;
    v.winner = phase_ == Phase::GameOver ? winner_ : 0;
    v.flash = 1.0;
    if (phase_ == Phase::GameOver) {
        v.flash = std::fmod(timer_ * 3.0, 1.0) < 0.5 ? 1.0 : 0.25;
    }
    v.flying = phase_ == Phase::Flying;
    return v;
}

PongView PongSim::step(double dt, const PongSettings& settings, bool serve, bool beat, PongEvents& events) {
    if (!seeded_ || seededWith_ != settings.seed) {
        seeded_ = true;
        seededWith_ = settings.seed;
        rng_ = settings.seed * 0x2545F4914F6CDD1DULL + 12345ULL;
        serveToward_ = uniform() < 0.5 ? -1 : 1;
        timer_ = settings.serveDelay;  // der erste Aufschlag wartet ebenfalls
    }
    accumulator_ += std::clamp(dt, 0.0, kMaxFrame);
    bool first = true;
    while (accumulator_ >= kStep) {
        accumulator_ -= kStep;
        substep(kStep, settings, serve && first, beat && first, events);
        first = false;
    }
    if (first && (serve || beat)) {
        // Frame ohne vollen Schritt (sehr kleine dt): Signale nicht verlieren, mit Nullzeit auswerten.
        substep(0.0, settings, serve, beat, events);
    }
    return view();
}

void PongSim::substep(double dt, const PongSettings& s, bool serve, bool beat, PongEvents& events) {
    switch (phase_) {
        case Phase::Waiting:
            timer_ -= dt;
            ballX_ = 0.5;
            ballY_ = 0.5;
            if (serve || (s.autoServe && timer_ <= 0.0 && (!s.beatMode || beat))) {
                launch(s);
            }
            break;
        case Phase::Flying:
            flight(dt, s, events);
            break;
        case Phase::GameOver:
            timer_ += dt;
            if (serve || (s.restart && timer_ >= s.winHold)) {
                scoreLeft_ = 0;
                scoreRight_ = 0;
                winner_ = 0;
                phase_ = Phase::Waiting;
                timer_ = s.serveDelay;
            }
            break;
    }
    movePaddles(dt, s);
}

void PongSim::launch(const PongSettings& s) {
    phase_ = Phase::Flying;
    ballX_ = 0.5;
    ballY_ = 0.5;
    const double dist = std::max(1.0 - 2.0 * s.margin, 0.1);
    speedNow_ = s.beatMode ? dist * std::max(s.bpm, 1.0) / 60.0 / std::max(s.beatsPerCrossing, 0.25)
                           : std::max(s.speed, 0.05);
    const double angle = (uniform() - 0.5) * 0.7;
    vx_ = serveToward_ * speedNow_ * (s.beatMode ? 1.0 : std::cos(angle));
    vy_ = s.beatMode ? vx_ * std::tan(angle) * serveToward_ : speedNow_ * std::sin(angle);
    planLeg(s);
}

void PongSim::planLeg(const PongSettings& s) {
    // Wohin kommt der Ball an der gegenüberliegenden Schlägerlinie, und soll der Rechner ihn dort erreichen?
    const bool toRight = vx_ > 0.0;
    const double plane = toRight ? 1.0 - s.margin : s.margin;
    if (std::abs(vx_) < 1e-9) {
        return;
    }
    const double travel = (plane - ballX_) / vx_;
    const double lo = s.ballRadius;
    const double hi = 1.0 - s.ballRadius;
    const double arrive = fold(ballY_ + vy_ * travel, lo, hi);
    const int control = toRight ? s.rightControl : s.leftControl;
    double& target = toRight ? rightTarget_ : leftTarget_;
    (toRight ? leftTarget_ : rightTarget_) = 0.5;  // der andere Schläger geht zurück in die Mitte
    if (control != 0) {
        return;
    }
    const double half = s.paddleSize * 0.5;
    if (uniform() < std::clamp(s.skill, 0.0, 1.0)) {
        target = clampPaddle(arrive + (uniform() - 0.5) * s.paddleSize * 0.7, s.paddleSize);
        return;
    }
    // Verfehlen: so weit daneben, dass der Ball sicher vorbeigeht, auf der Seite mit Platz.
    const double need = half + s.ballRadius + 0.03;
    const double upper = arrive + need;
    const double lower = arrive - need;
    const bool upperOk = upper <= 1.0 - half;
    const bool lowerOk = lower >= half;
    if (upperOk && (!lowerOk || uniform() < 0.5)) {
        target = upper;
    } else if (lowerOk) {
        target = lower;
    } else {
        target = clampPaddle(arrive, s.paddleSize);  // kein Platz zum Verfehlen: der Schläger trifft
    }
}

void PongSim::movePaddles(double dt, const PongSettings& s) {
    const auto move = [&](double& paddle, double target, int control, double input) {
        if (control != 0) {
            paddle = clampPaddle(input, s.paddleSize);
            return;
        }
        const double reach = std::max(s.paddleSpeed, 0.01) * dt;
        paddle += std::clamp(target - paddle, -reach, reach);
        paddle = clampPaddle(paddle, s.paddleSize);
    };
    move(left_, leftTarget_, s.leftControl, s.leftInput);
    move(right_, rightTarget_, s.rightControl, s.rightInput);
}

void PongSim::flight(double dt, const PongSettings& s, PongEvents& events) {
    ballX_ += vx_ * dt;
    ballY_ += vy_ * dt;
    if (ballY_ < s.ballRadius) {
        ballY_ = 2.0 * s.ballRadius - ballY_;
        vy_ = -vy_;
        events.wallHit = true;
    } else if (ballY_ > 1.0 - s.ballRadius) {
        ballY_ = 2.0 * (1.0 - s.ballRadius) - ballY_;
        vy_ = -vy_;
        events.wallHit = true;
    }
    const auto bounce = [&](bool rightSide) {
        const double paddle = rightSide ? right_ : left_;
        const double offset = (ballY_ - paddle) / std::max(s.paddleSize * 0.5, 1e-6);
        if (std::abs(ballY_ - paddle) > s.paddleSize * 0.5 + s.ballRadius * 0.5) {
            return false;  // verfehlt
        }
        const double angle = std::clamp(offset, -1.0, 1.0) * s.maxAngle;
        if (!s.beatMode) {
            const double base = std::max(s.speed, 0.05);
            speedNow_ = std::min(speedNow_ * (1.0 + std::max(s.speedUp, 0.0)), base * kMaxSpeedUp);
        }
        const double dir = rightSide ? -1.0 : 1.0;
        vx_ = dir * (s.beatMode ? std::abs(vx_) : speedNow_ * std::cos(angle));
        vy_ = s.beatMode ? std::abs(vx_) * std::tan(angle) : speedNow_ * std::sin(angle);
        ballX_ = rightSide ? 1.0 - s.margin : s.margin;
        events.paddleHit = true;
        planLeg(s);
        return true;
    };
    if (vx_ > 0.0 && ballX_ >= 1.0 - s.margin && ballX_ - vx_ * dt < 1.0 - s.margin) {
        bounce(true);
    } else if (vx_ < 0.0 && ballX_ <= s.margin && ballX_ - vx_ * dt > s.margin) {
        bounce(false);
    }
    if (ballX_ > 1.0) {
        score(1, s, events);
    } else if (ballX_ < 0.0) {
        score(2, s, events);
    }
}

void PongSim::score(int side, const PongSettings& s, PongEvents& events) {
    events.point = true;
    (side == 1 ? scoreLeft_ : scoreRight_) += 1;
    ballX_ = 0.5;
    ballY_ = 0.5;
    serveToward_ = side == 1 ? 1 : -1;  // wer den Punkt verloren hat, bekommt den Ball
    if ((side == 1 ? scoreLeft_ : scoreRight_) >= std::max(s.maxScore, 1)) {
        phase_ = Phase::GameOver;
        winner_ = side;
        timer_ = 0.0;
        return;
    }
    phase_ = Phase::Waiting;
    timer_ = s.serveDelay;
    leftTarget_ = 0.5;
    rightTarget_ = 0.5;
}

}  // namespace whitehole::nodes::detail
