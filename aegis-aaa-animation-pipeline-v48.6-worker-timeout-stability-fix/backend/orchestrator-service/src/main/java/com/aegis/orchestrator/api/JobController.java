package com.aegis.orchestrator.api;

import com.aegis.common.dto.ContinueJobRequest;
import com.aegis.common.dto.PipelineRunRequest;
import com.aegis.common.model.JobState;
import com.aegis.orchestrator.service.JobStore;
import com.aegis.orchestrator.service.PipelineService;
import jakarta.validation.Valid;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.List;
import java.util.Map;

@RestController
@RequestMapping("/api/v1")
public class JobController {
    private final PipelineService pipelineService;
    private final JobStore jobStore;

    public JobController(PipelineService pipelineService, JobStore jobStore) {
        this.pipelineService = pipelineService;
        this.jobStore = jobStore;
    }

    @GetMapping("/health")
    public Map<String, Object> health() {
        return Map.of("service", "aegis-orchestrator-service", "version", "V47", "status", "ok");
    }

    @PostMapping("/pipelines/bandai/run")
    public JobState startBandai(@Valid @RequestBody PipelineRunRequest request) {
        return pipelineService.startBandaiPipeline(request);
    }

    @PostMapping("/jobs/{jobId}/continue-after-retarget")
    public JobState continueAfterRetarget(@PathVariable("jobId") String jobId, @RequestBody(required = false) ContinueJobRequest request) {
        return pipelineService.continueAfterRetarget(jobId, request == null ? null : request.note());
    }

    @GetMapping("/jobs")
    public List<JobState> listJobs() { return jobStore.listAll(); }

    @DeleteMapping("/jobs")
    public Map<String, Object> deleteJobs(@RequestParam(defaultValue = "true") boolean terminalOnly) {
        return jobStore.deleteAll(terminalOnly);
    }

    @GetMapping("/jobs/{jobId}")
    public ResponseEntity<JobState> getJob(@PathVariable("jobId") String jobId) {
        return jobStore.find(jobId).map(ResponseEntity::ok).orElse(ResponseEntity.notFound().build());
    }

    @DeleteMapping("/jobs/{jobId}")
    public ResponseEntity<Map<String, Object>> deleteJob(@PathVariable("jobId") String jobId) {
        boolean deleted = jobStore.delete(jobId);
        if (!deleted) return ResponseEntity.notFound().build();
        return ResponseEntity.ok(Map.of("deleted", true, "jobId", jobId));
    }

    @GetMapping("/jobs/{jobId}/logs")
    public ResponseEntity<?> getLogs(@PathVariable("jobId") String jobId) {
        return jobStore.find(jobId)
                .map(state -> ResponseEntity.ok(state.getLogs()))
                .orElse(ResponseEntity.notFound().build());
    }

    @GetMapping("/jobs/{jobId}/logs/tail")
    public ResponseEntity<List<String>> getLogTail(@PathVariable("jobId") String jobId,
                                                   @RequestParam(defaultValue = "80") int limit) {
        if (jobStore.find(jobId).isEmpty()) return ResponseEntity.notFound().build();
        return ResponseEntity.ok(jobStore.tailLogs(jobId, limit));
    }

    @GetMapping("/exports/latest")
    public Map<String, Object> latestExport() { return Map.of("latestExport", jobStore.latestExport()); }
}
