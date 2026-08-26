# Chapter 7.7: Schema Validators

Maintaining schema integrity is crucial for game development. Validators ensure that schemas and their instances conform to expected patterns, preventing errors before they propagate through the pipeline. This section explores strategies for implementing effective schema validation.

## Validator Types

Different validation needs require different types of validators:

1. **Schema Definition Validators**: Ensure schemas themselves are well-formed
2. **Instance Validators**: Check that schema instances have valid property values
3. **Consistency Validators**: Verify relationships between related schemas
4. **Performance Validators**: Identify potential performance issues
5. **Style Validators**: Enforce naming conventions and best practices

Each validator type addresses a different aspect of schema quality, and a comprehensive validation system should include all types.

## Schema Definition Validators

Schema definition validators check the schemas themselves for correctness:

1. Proper namespace usage
2. Consistent naming conventions
3. Appropriate property types and defaults
4. Proper inheritance structure
5. Clear documentation

The code sample [`schema_definition_validator.py`](code_samples/schema_definition_validator.py) demonstrates a validator that checks schema definitions for conformance to best practices. This implementation helps maintain consistent, high-quality schema definitions.

## Instance Validators

Instance validators check that schema properties have valid values:

1. Value range validation (min/max)
2. Enum/token validation (allowed values)
3. Reference validation (missing targets)
4. Required property validation
5. Type-specific validation (unit vectors, etc.)

The code sample [`schema_instance_validator.py`](code_samples/schema_instance_validator.py) demonstrates a validator for checking schema instances against their definitions. This implementation helps catch issues like out-of-range values or invalid references.

## Consistency Validators

Consistency validators check relationships between entities:

1. Parent-child relationship validation
2. Reference integrity checking
3. Schema composition validation
4. Dependency validation
5. System-specific rules (physics, animation, etc.)

The code sample [`schema_consistency_validator.py`](code_samples/schema_consistency_validator.py) demonstrates a validator for checking the consistency of related entities. This implementation helps ensure that schema instances work together correctly as a system.

## Performance Validators

Performance validators identify potential runtime issues:

1. Excessive property counts
2. Deep composition chains
3. Inefficient access patterns
4. Memory usage warnings
5. Processing cost estimates

The code sample [`schema_performance_validator.py`](code_samples/schema_performance_validator.py) demonstrates a validator that checks for potential performance issues in schema usage. This implementation helps prevent performance problems before they impact production.

## Rule-Based Validation Framework

A flexible validation framework allows custom rule definition:

1. Declarative rule definition syntax
2. Severity levels for issues (error, warning, info)
3. Context-sensitive validation
4. Rule categories and filtering
5. Progressive validation (fail fast vs. collect all issues)

The code sample [`schema_validation_framework.py`](code_samples/schema_validation_framework.py) demonstrates a flexible framework for defining and applying validation rules. This implementation allows teams to create custom validation rules specific to their project needs.

## Integration Points

Validators should integrate with existing workflows at key points:

1. **Authoring Time**: Immediate feedback in DCC tools
2. **Save Time**: Validation on file save operations
3. **Import Time**: Validation when data enters the pipeline
4. **Build Time**: Validation during content builds
5. **Runtime**: Optional validation during development gameplay

The code sample [`schema_validation_integrations.py`](code_samples/schema_validation_integrations.py) demonstrates how to integrate validation at different points in the content pipeline. This implementation ensures that validation occurs at the most appropriate times.

## Error Reporting and Fixes

Effective validators not only detect issues but help resolve them:

1. Clear, context-aware error messages
2. Suggested fixes for common issues
3. Automatic fix application where safe
4. Documentation links for complex issues
5. Visual indicators of problem locations

The code sample [`schema_validation_reporter.py`](code_samples/schema_validation_reporter.py) demonstrates a system for reporting validation issues and suggesting fixes. This implementation helps users understand and resolve schema problems quickly.

## Validation Exemptions

Sometimes valid use cases require exempting specific validation rules:

1. Per-instance exemption flags
2. Comment-based exemption directives
3. Global exemption registry
4. Temporary exemption windows during transitions
5. Exemption audit logs

The code sample [`schema_validation_exemptions.py`](code_samples/schema_validation_exemptions.py) demonstrates a system for managing validation exemptions. This implementation provides flexibility while maintaining accountability.

## Game-Specific Validators

Game-specific validators enforce rules particular to a project:

1. Level design constraints
2. Gameplay balance rules
3. Performance budgets
4. IP compliance rules
5. Platform-specific limitations

The code sample [`sparkle_carrot_validators.py`](code_samples/sparkle_carrot_validators.py) demonstrates game-specific validators for our SparkleCarrot example game. This implementation shows how validation can enforce rules specific to a particular game's design.

## Key Takeaways

- Different validator types address different quality aspects
- A flexible validation framework enables custom rules
- Integration at multiple pipeline stages catches issues early
- Clear error reporting helps users resolve issues quickly
- Exemption mechanisms provide flexibility when needed
- Game-specific validators enforce project-specific rules

By implementing comprehensive schema validation, teams can catch issues early, maintain high-quality standards, and prevent problems from propagating through the pipeline. Effective validators become a valuable assistant rather than a burdensome gatekeeper, helping team members create better content with less rework.
