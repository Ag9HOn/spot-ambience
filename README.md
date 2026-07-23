# Spot Ambience

Spot Ambience is a Sailfish OS application that follows Finland's quarter-hour
electricity prices and can select an installed ambience from the average price
of the current and following three quarters.

Created by Petteri Nakamura with ChatGPT Codex.
Project source: https://github.com/Ag9HOn/spot-ambience

It provides:

- current and next-hour prices;
- quarter-hour tables and hourly, seven-day, and 31-day graphs;
- Active Cover price display;
- configurable VAT, transfer-price additions, and price thresholds;
- Kaamos, Snow White, Inari Blue, Ruska, and Flare defaults when installed;
- missing-ambience fallbacks and a quiet-hours override;
- an optional test cycle that changes ambience every 15 minutes.

Automation is disabled after installation. Enable it in Settings and save.
The application uses short-lived sandboxed background runs where the installed
Sailfish OS exposes the required Timed permission. On devices where that
permission is not available, price display and manual refresh continue to
work, while background scheduling reports the platform limitation.

## Prices

Spot price data: Nord Pool day-ahead prices for Finland, published through
[Elering LIVE](https://dashboard.elering.ee/) by Elering AS. Elering is
Estonia's state-owned electricity and gas transmission-system operator, and
its public service supports automated data use. The app requests Finland's
raw, pre-VAT prices in 15-minute intervals and keeps a local cache for charts
and offline display. VAT, transfer-price additions, and electricity tax are
calculated locally.

The app normally refreshes once per day at 14:15 Europe/Helsinki and retries
later when tomorrow's prices are not yet complete. It does not provide a public
data API or bulk export. Endpoint details are in
[docs/DATA-SOURCE.md](docs/DATA-SOURCE.md).

Spot Ambience is a free community project with no account, analytics, or
advertising. It is not affiliated with Elering or Nord Pool. Prices may be
delayed, corrected, unavailable, or inaccurate; the application is provided
as-is and is not financial or energy advice.

## Installation

The release RPM uses the normal Sailfish package installer. First launch
requests Internet and Ambience permissions. Without Ambience access, price
display remains available while ambience inventory and switching remain off.

## Transfer price

Optional transfer-price settings support one c/kWh price or separate day and
night prices, plus electricity tax in its own field. Night price applies from
22:00 until 07:00 in the phone timezone. Transfer price and tax are added after
the optional VAT calculation as final c/kWh additions.

## Legal

The software is provided as-is. The source release includes [LICENSE](LICENSE),
[NOTICE](NOTICE.md), and [PRIVACY](PRIVACY.md).
