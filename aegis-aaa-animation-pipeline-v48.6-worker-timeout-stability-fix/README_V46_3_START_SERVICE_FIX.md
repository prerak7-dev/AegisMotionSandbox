# Aegis V46.3 Start Service Fix

This patch fixes the common failure when running:

```powershell
.\scripts\v46-start-orchestrator.ps1
```

## Root cause

The previous script ran:

```powershell
mvn -pl orchestrator-service -am spring-boot:run
```

`-am` also selects dependency modules such as `common`. Maven can then try to run
`spring-boot:run` across modules that are not Spring Boot applications, which can produce
errors such as:

```text
Unable to find a suitable main class
Unable to find main class
```

## Fix

The start scripts now run from the individual service folders:

```powershell
backend\orchestrator-service
backend\worker-service
```

and call:

```powershell
mvn spring-boot:run
```

The service POMs also explicitly specify their main classes:

```text
com.aegis.orchestrator.AegisOrchestratorApplication
com.aegis.worker.AegisWorkerApplication
```

## Correct run order

```powershell
.\scripts\v46-start-infra.ps1
.\scripts\v46-build-backend.ps1
.\scripts\v46-start-orchestrator.ps1
```

In another PowerShell window:

```powershell
.\scripts\v46-start-worker.ps1
```
