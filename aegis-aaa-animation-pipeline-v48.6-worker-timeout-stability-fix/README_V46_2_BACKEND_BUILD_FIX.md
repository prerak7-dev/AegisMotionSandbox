# Aegis V46.2 Backend Build Fix

Fixes the Maven build error in `worker-service`:

```text
package com.fasterxml.jackson.core does not exist
package com.fasterxml.jackson.databind does not exist
cannot find symbol ObjectMapper
cannot find symbol JsonProcessingException
```

## Root cause

`WorkerJobStore` serializes job state to Redis using Jackson (`ObjectMapper`,
`JsonProcessingException`), but `worker-service` did not explicitly include Jackson on its
classpath.

## Fixes

- Added `jackson-databind` to `backend/worker-service/pom.xml`.
- Added `jackson-datatype-jsr310` to support Java `Instant` serialization.
- Added an explicit `JacksonConfig` bean to `worker-service`.
- Added matching `jackson-datatype-jsr310` and explicit `JacksonConfig` to `orchestrator-service`
  for consistent Redis serialization.

## Rebuild

From the pipeline root:

```powershell
.\scripts\v46-build-backend.ps1
```

Or resume Maven from the failed module:

```powershell
cd backend
mvn clean install
```
