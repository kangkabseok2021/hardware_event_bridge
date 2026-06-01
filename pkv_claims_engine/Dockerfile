FROM ubuntu:24.04 AS build
RUN apt-get update -qq && apt-get install -y --no-install-recommends \
    cmake make g++ pkg-config libpq-dev libpqxx-dev ca-certificates && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY . .
RUN cmake -B build/pkv -S pkv_claims_engine \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_POLICY_DEFAULT_CMP0135=NEW && \
    cmake --build build/pkv --target pkv_claims_engine -j$(nproc)

FROM ubuntu:24.04
RUN apt-get update -qq && apt-get install -y --no-install-recommends \
    libpq5 libpqxx-7.9 && rm -rf /var/lib/apt/lists/*
COPY --from=build /app/build/pkv/pkv_claims_engine /usr/local/bin/pkv_claims_engine
EXPOSE 8080
ENTRYPOINT ["pkv_claims_engine"]
