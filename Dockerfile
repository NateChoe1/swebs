FROM debian:stable-slim AS build
RUN apt-get update -y && apt-get upgrade -y && apt-get install -y libgnutls28-dev libgnutls30 gcc make pkg-config

COPY . /swebs
WORKDIR /swebs
RUN make && make install

ENTRYPOINT [ "swebs", "-s", "/site/sitefile" ]
