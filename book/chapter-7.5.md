# Chapter 7.5: Schema Request System

As game development progresses, teams inevitably need to extend or modify schemas to accommodate new gameplay features. A formal schema request system provides structure for this process, ensuring changes are well-considered, properly reviewed, and efficiently implemented. This section explores approaches to establishing an effective schema request workflow.

## Formal Request Process

A structured schema request process helps manage changes efficiently:

1. **Request Submission**: Designers submit detailed requirements
2. **Technical Review**: TD or programmer evaluates feasibility
3. **Prioritization**: Requests are prioritized based on production needs
4. **Implementation**: Approved schemas are developed and tested
5. **Deployment**: New schemas are versioned and distributed

This process strikes a balance between accommodating creative needs and maintaining technical discipline.

The document template [`schema_request_template.md`](code_samples/schema_request_template.md) provides a standardized format for submitting schema change requests. This template ensures that all necessary information is captured upfront.

## Web-Based Request Portal

A dedicated web application for schema requests offers several advantages:

1. Centralized tracking of requests and their status
2. Searchable history of previous requests
3. Discussion threads for collaborative refinement
4. Integration with task tracking systems
5. Email notifications for status changes

The code sample [`schema_request_app.py`](code_samples/schema_request_app.py) demonstrates a simple Flask-based web application for managing schema requests. This implementation provides a user-friendly interface for submitting and tracking schema change requests.

## Request Evaluation Criteria

Clear criteria help technical directors evaluate schema requests consistently:

1. **Gameplay Necessity**: Is this truly needed for gameplay?
2. **Technical Feasibility**: Can it be implemented efficiently?
3. **Performance Impact**: Will it affect runtime performance?
4. **Migration Complexity**: How difficult will it be to update existing assets?
5. **Pipeline Integration**: Does it work with existing tools?

The document [`schema_evaluation_checklist.md`](code_samples/schema_evaluation_checklist.md) provides a detailed checklist for evaluating schema requests. This checklist helps technical directors assess the impact and feasibility of proposed changes.

## Request Workflow Integration

Schema request systems should integrate with existing production tools:

1. Integration with task tracking (Jira, etc.)
2. Source control system hooks
3. Connection to CI/CD pipelines
4. Documentation system integration
5. Communication channels (Slack, Teams, etc.)

The code sample [`schema_request_integrations.py`](code_samples/schema_request_integrations.py) demonstrates integration points with common production tools. This implementation shows how to connect schema requests with task tracking and communication systems.

## Request Prioritization Framework

With limited technical resources, prioritizing schema requests is essential:

1. **Impact**: How many assets/systems will benefit?
2. **Urgency**: Is this blocking production?
3. **Effort**: How complex is the implementation?
4. **Risk**: What could go wrong?
5. **Dependencies**: Does this enable other features?

The template [`schema_prioritization_matrix.xlsx`](code_samples/schema_prioritization_matrix.xlsx) provides a scoring system for prioritizing schema requests. This matrix helps teams objectively evaluate which requests should be addressed first.

## Request Implementation Tracking

Once approved, schema request implementation should be tracked:

1. Development status tracking
2. Test validation
3. Documentation updates
4. Release planning
5. Deployment confirmation

The code sample [`schema_implementation_tracker.py`](code_samples/schema_implementation_tracker.py) demonstrates a system for tracking schema implementation progress. This tool helps teams monitor the status of in-progress schema changes.

## User Feedback Loop

A critical part of the request system is soliciting feedback after implementation:

1. Verify that implemented schemas meet the original requirements
2. Gather feedback on schema usability
3. Collect data on performance and stability
4. Identify any needed refinements
5. Document lessons for future schema development

The template [`schema_implementation_feedback_form.md`](code_samples/schema_implementation_feedback_form.md) provides a standardized format for gathering feedback on implemented schemas. This feedback loop helps improve both the schemas and the request process itself.

## Case Study: Small vs. Large Team Approaches

Schema request processes should scale with team size:

### Small Team Approach
- Simple issue tracking in GitHub/GitLab
- Direct communication between designers and technical staff
- Weekly review meetings for pending requests
- Shared documentation in cloud storage
- Minimal formal process overhead

### Large Team Approach
- Dedicated web portal for requests
- Multi-stage approval workflow
- Formal technical review committee
- Detailed implementation specifications
- Comprehensive documentation and training

The document [`schema_request_scaling.md`](code_samples/schema_request_scaling.md) provides detailed guidelines for scaling the schema request process based on team size and project complexity.

## Key Takeaways

- Formal request processes provide structure without stifling creativity
- Clear evaluation criteria ensure consistent decision-making
- Integration with existing tools improves adoption
- Prioritization frameworks help manage limited resources
- Implementation tracking ensures requests don't get lost
- Feedback loops drive continuous improvement
- Processes should scale with team size and project needs

An effective schema request system balances the need for creative freedom with technical discipline, allowing game teams to evolve their data model while maintaining quality and performance. The ideal system feels like a helpful resource rather than bureaucratic overhead.
