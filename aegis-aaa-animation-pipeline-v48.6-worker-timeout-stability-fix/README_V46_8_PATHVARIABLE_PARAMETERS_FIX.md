# Aegis V46.8 — PathVariable / `-parameters` Fix

Fixes this Spring Boot error:

```text
Name for argument of type [java.lang.String] not specified,
and parameter name information not available via reflection.
Ensure that the compiler uses the '-parameters' flag.
```

## Root cause

Spring MVC could not infer the name of this route parameter:

```java
@GetMapping("/jobs/{jobId}")
public ResponseEntity<JobState> getJob(@PathVariable String jobId)
```

because Java parameter-name metadata was not available at runtime.

## Fixes

1. Explicitly named all `@PathVariable` bindings:

```java
@PathVariable("jobId") String jobId
```

2. Added Maven compiler support for parameter metadata:

```xml
<parameters>true</parameters>
```

in the parent backend `pom.xml`.

## Required steps

After replacing the pipeline with V46.8:

```powershell
.\scripts\v46-build-backend.ps1
```

Then restart the orchestrator and worker services:

```powershell
.\scripts\v46-start-orchestrator.ps1
.\scripts\v46-start-worker.ps1
```

Then retry:

```powershell
.\scripts\v46-get-job.ps1 -JobId "<jobId>"
```

or open:

```text
http://localhost:8088/api/v1/jobs/<jobId>
```
