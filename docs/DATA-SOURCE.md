# Price data

Project source: https://github.com/Ag9HOn/spot-ambience

Spot price data: Nord Pool day-ahead prices for Finland, published through
[Elering LIVE](https://dashboard.elering.ee/) by
[Elering AS](https://elering.ee/en/about-elering). Elering is Estonia's
state-owned electricity and gas transmission-system operator. Elering LIVE is
used for automated low-volume retrieval in this free community application.

## Integration

The application retrieves data from:

`https://dashboard.elering.ee/api/nps/price?start=<UTC>&end=<UTC>`

It reads only the `data.fi` series from a successful response. Each entry has:

- `timestamp`: UTC Unix seconds, aligned to a 15-minute market interval;
- `price`: raw EUR/MWh, before the application's optional VAT calculation.

The application converts raw prices to c/kWh locally and then applies the
user's chosen VAT, transfer price, and electricity tax. It retains local price
history for graphs and offline display, refreshes once per day at 14:15
Europe/Helsinki, and retries later when tomorrow's prices are incomplete. It
does not expose a public data API or bulk export.

Prices may be delayed, corrected, unavailable, or inaccurate. The application
is provided as-is and makes no guarantee about the data or its availability.
