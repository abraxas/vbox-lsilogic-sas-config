FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends python3 gcc libc6-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /lab

COPY gadget.c /lab/gadget.c
COPY poc.py /lab/poc.py

CMD ["python3", "/lab/poc.py"]
