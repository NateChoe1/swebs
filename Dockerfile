FROM debian:stable-slim AS build
RUN apt-get update -y && apt-get upgrade -y && apt-get install -y libgnutls28-dev libgnutls30 gcc make pkg-config
COPY . /swebs
WORKDIR /swebs
RUN make

FROM debian:stable-slim AS run
RUN apt-get update -y && apt-get upgrade -y && apt-get install -y libgnutls28-dev libgnutls30

COPY --from=build /swebs/build/swebs /usr/sbin/swebs
RUN mkdir /usr/include/swebs
COPY --from=build /swebs/src/swebs /usr/include/swebs/

RUN useradd -M swebs

ENTRYPOINT [ "swebs", "-s", "/site/sitefile" ]
