#!/usr/bin/env python
"""
schema_validator.py

A validator for SparkleCarrot game schemas that checks for correctness,
consistency, and performance issues.
"""

from pxr import Usd, UsdGeom, Sdf, Tf
import argparse
import sys
import os
import re
import json
from enum import Enum
from typing import List, Dict, Any, Optional, Tuple, Set


class ValidationSeverity(Enum):
    """Severity levels for validation issues"""
    ERROR = 0
    WARNING = 1
    INFO = 2


class ValidationIssue:
    """Represents a single validation issue"""
    
    def __init__(self, path: Sdf.Path, message: str, severity: ValidationSeverity,
                 rule_id: str, fix_description: Optional[str] = None):
        self.path = path
        self.message = message
        self.severity = severity
        self.rule_id = rule_id
        self.fix_description = fix_description
    
    def __str__(self) -> str:
        severity_str = {
            ValidationSeverity.ERROR: "ERROR",
            ValidationSeverity.WARNING: "WARNING",
            ValidationSeverity.INFO: "INFO"
        }[self.severity]
        
        result = f"{severity_str} [{self.rule_id}] at {self.path}: {self.message}"
        if self.fix_description:
            result += f"\n  Suggested fix: {self.fix_description}"
        
        return result


class ValidationRule:
    """Base class for validation rules"""
    
    def __init__(self, rule_id: str, description: str):
        self.rule_id = rule_id
        self.description = description
    
    def validate(self, stage: Usd.Stage, prim: Usd.Prim) -> List[ValidationIssue]:
        """Validate a prim against this rule"""
        raise NotImplementedError("Subclasses must implement validate method")


class NamespaceValidationRule(ValidationRule):
    """Rule for validating property namespace conventions"""
    
    def __init__(self):
        super().__init__(
            "SCHEMA_NAMESPACE_001",
            "Property names should follow the namespace convention 'sparkle:category:name'"
        )
        
        # Regex pattern for valid namespaced attributes
        self.namespace_pattern = re.compile(r'^sparkle:[a-zA-Z]+:[a-zA-Z][a-zA-Z0-9_]*$')
    
    def validate(self, stage: Usd.Stage, prim: Usd.Prim) -> List[ValidationIssue]:
        issues = []
        
        # Only check attributes where namespace should be enforced
        for attr in prim.GetAttributes():
            name = attr.GetName()
            
            # Skip non-sparkle attributes
            if not name.startswith("sparkle:"):
                continue
            
            # Check namespace pattern
            if not self.namespace_pattern.match(name):
                issues.append(ValidationIssue(
                    prim.GetPath(),
                    f"Attribute '{name}' does not follow namespace convention",
                    ValidationSeverity.ERROR,
                    self.rule_id,
                    f"Rename to follow pattern 'sparkle:category:name'"
                ))
        
        return issues


class HealthComponentValidationRule(ValidationRule):
    """Rule for validating the health component"""
    
    def __init__(self):
        super().__init__(
            "HEALTH_COMP_001",
            "Health component should have required attributes with valid values"
        )
    
    def validate(self, stage: Usd.Stage, prim: Usd.Prim) -> List[ValidationIssue]:
        issues = []
        
        # Check if prim has the health API schema
        schemas = []
        prim.GetAppliedSchemas(schemas)
        
        if "SparkleHealthAPI" not in schemas:
            return issues  # Not applicable
        
        # Check required attributes
        required_attrs = {
            "sparkle:health:current": {"type": Sdf.ValueTypeNames.Float, "min": 0.0},
            "sparkle:health:maximum": {"type": Sdf.ValueTypeNames.Float, "min": 0.0},
        }
        
        for attr_name, requirements in required_attrs.items():
            attr = prim.GetAttribute(attr_name)
            
            # Check attribute existence
            if not attr:
                issues.append(ValidationIssue(
                    prim.GetPath(),
                    f"Health component missing required attribute '{attr_name}'",
                    ValidationSeverity.ERROR,
                    self.rule_id,
                    f"Add attribute '{attr_name}' with appropriate value"
                ))
                continue
            
            # Check attribute type
            if attr.GetTypeName() != requirements["type"]:
                issues.append(ValidationIssue(
                    prim.GetPath(),
                    f"Attribute '{attr_name}' has incorrect type '{attr.GetTypeName()}', expected '{requirements['type']}'",
                    ValidationSeverity.ERROR,
                    self.rule_id,
                    f"Change attribute type to '{requirements['type']}'"
                ))
            
            # Check value constraints
            if "min" in requirements:
                value = 0.0
                if attr.Get(&value) and value < requirements["min"]:
                    issues.append(ValidationIssue(
                        prim.GetPath(),
                        f"Attribute '{attr_name}' value {value} is less than minimum {requirements['min']}",
                        ValidationSeverity.ERROR,
                        self.rule_id,
                        f"Set value to at least {requirements['min']}"
                    ))
        
        # Check value relationships
        current_health = prim.GetAttribute("sparkle:health:current")
        max_health = prim.GetAttribute("sparkle:health:maximum")
        
        if current_health and max_health:
            current_value = 0.0
            max_value = 0.0
            
            if current_health.Get(&current_value) and max_health.Get(&max_value):
                if current_value > max_value:
                    issues.append(ValidationIssue(
                        prim.GetPath(),
                        f"Current health ({current_value}) exceeds maximum health ({max_value})",
                        ValidationSeverity.ERROR,
                        self.rule_id,
                        f"Reduce current health to be less than or equal to maximum health"
                    ))
        
        return issues


class MovementComponentValidationRule(ValidationRule):
    """Rule for validating the movement component"""
    
    def __init__(self):
        super().__init__(
            "MOVEMENT_COMP_001",
            "Movement component should have valid pattern and speed values"
        )
    
    def validate(self, stage: Usd.Stage, prim: Usd.Prim) -> List[ValidationIssue]:
        issues = []
        
        # Check if prim has the movement API schema
        schemas = []
        prim.GetAppliedSchemas(schemas)
        
        if "SparkleMovementAPI" not in schemas:
            return issues  # Not applicable
        
        # Check speed attribute
        speed_attr = prim.GetAttribute("sparkle:movement:speed")
        if not speed_attr:
            issues.append(ValidationIssue(
                prim.GetPath(),
                "Movement component missing required attribute 'sparkle:movement:speed'",
                ValidationSeverity.ERROR,
                self.rule_id,
                "Add 'sparkle:movement:speed' attribute with appropriate value"
            ))
        else:
            # Check speed value
            speed = 0.0
            if speed_attr.Get(&speed):
                if speed < 0.0:
                    issues.append(ValidationIssue(
                        prim.GetPath(),
                        f"Movement speed ({speed}) cannot be negative",
                        ValidationSeverity.ERROR,
                        self.rule_id,
                        "Set speed to a non-negative value"
                    ))
                elif speed > 100.0:
                    issues.append(ValidationIssue(
                        prim.GetPath(),
                        f"Movement speed ({speed}) exceeds maximum reasonable value (100)",
                        ValidationSeverity.WARNING,
                        self.rule_id,
                        "Consider reducing speed unless extreme speed is intentional"
                    ))
        
        # Check pattern attribute
        pattern_attr = prim.GetAttribute("sparkle:movement:pattern")
        if not pattern_attr:
            issues.append(ValidationIssue(
                prim.GetPath(),
                "Movement component missing required attribute 'sparkle:movement:pattern'",
                ValidationSeverity.WARNING,
                self.rule_id,
                "Add 'sparkle:movement:pattern' attribute with appropriate value"
            ))
        else:
            # Check pattern value
            pattern = Tf.Token()
            if pattern_attr.Get(&pattern):
                allowed_patterns = ["direct", "patrol", "wander", "charge", "flee", "stationary"]
                if pattern.GetString() not in allowed_patterns:
                    issues.append(ValidationIssue(
                        prim.GetPath(),
                        f"Movement pattern '{pattern}' is not one of the allowed values: {', '.join(allowed_patterns)}",
                        ValidationSeverity.ERROR,
                        self.rule_id,
                        f"Set pattern to one of: {', '.join(allowed_patterns)}"
                    ))
        
        return issues


class AiComponentValidationRule(ValidationRule):
    """Rule for validating the AI component"""
    
    def __init__(self):
        super().__init__(
            "AI_COMP_001",
            "AI component should have valid behavior values and consistent configuration"
        )
    
    def validate(self, stage: Usd.Stage, prim: Usd.Prim) -> List[ValidationIssue]:
        issues = []
        
        # Check if prim has the AI API schema
        schemas = []
        prim.GetAppliedSchemas(schemas)
        
        if "SparkleAIAPI" not in schemas:
            return issues  # Not applicable
        
        # Check behavior attribute
        behavior_attr = prim.GetAttribute("sparkle:ai:behavior")
        if not behavior_attr:
            issues.append(ValidationIssue(
                prim.GetPath(),
                "AI component missing required attribute 'sparkle:ai:behavior'",
                ValidationSeverity.ERROR,
                self.rule_id,
                "Add 'sparkle:ai:behavior' attribute with appropriate value"
            ))
        else:
            # Check behavior value
            behavior = Tf.Token()
            if behavior_attr.Get(&behavior):
                allowed_behaviors = ["passive", "defensive", "aggressive", "neutral", "flee"]
                if behavior.GetString() not in allowed_behaviors:
                    issues.append(ValidationIssue(
                        prim.GetPath(),
                        f"AI behavior '{behavior}' is not one of the allowed values: {', '.join(allowed_behaviors)}",
                        ValidationSeverity.ERROR,
                        self.rule_id,
                        f"Set behavior to one of: {', '.join(allowed_behaviors)}"
                    ))
        
        # Check detection radius attribute
        radius_attr = prim.GetAttribute("sparkle:ai:detectionRadius")
        if radius_attr:
            radius = 0.0
            if radius_attr.Get(&radius) and radius < 0.0:
                issues.append(ValidationIssue(
                    prim.GetPath(),
                    f"AI detection radius ({radius}) cannot be negative",
                    ValidationSeverity.ERROR,
                    self.rule_id,
                    "Set detection radius to a non-negative value"
                ))
        
        # Check for patrol path consistency
        patrol_path_rel = prim.GetRelationship("sparkle:ai:patrolPath")
        if patrol_path_rel:
            # If has patrol path, movement pattern should be "patrol"
            movement_pattern_attr = prim.GetAttribute("sparkle:movement:pattern")
            if movement_pattern_attr:
                pattern = Tf.Token()
                if movement_pattern_attr.Get(&pattern) and pattern.GetString() != "patrol":
                    issues.append(ValidationIssue(
                        prim.GetPath(),
                        f"AI has patrol path but movement pattern is '{pattern}' instead of 'patrol'",
                        ValidationSeverity.WARNING,
                        self.rule_id,
                        "Change movement pattern to 'patrol' to match patrol path relationship"
                    ))
        
        return issues


class PerformanceValidationRule(ValidationRule):
    """Rule for validating performance aspects of schemas"""
    
    def __init__(self):
        super().__init__(
            "PERF_001",
            "Schema usage should follow performance best practices"
        )
    
    def validate(self, stage: Usd.Stage, prim: Usd.Prim) -> List[ValidationIssue]:
        issues = []
        
        # Check applied API schemas count
        schemas = []
        prim.GetAppliedSchemas(schemas)
        
        if len(schemas) > 10:
            issues.append(ValidationIssue(
                prim.GetPath(),
                f"Prim has excessive number of API schemas applied ({len(schemas)})",
                ValidationSeverity.WARNING,
                self.rule_id,
                "Consider consolidating functionality to reduce schema count"
            ))
        
        # Check attribute count
        attrs = prim.GetAttributes()
        if len(attrs) > 50:
            issues.append(ValidationIssue(
                prim.GetPath(),
                f"Prim has excessive number of attributes ({len(attrs)})",
                ValidationSeverity.WARNING,
                self.rule_id,
                "Consider refactoring to reduce attribute count"
            ))
        
        # Check for expensive data types
        expensive_count = 0
        for attr in attrs:
            if attr.GetTypeName() in [Sdf.ValueTypeNames.Matrix4d, Sdf.ValueTypeNames.String]:
                expensive_count += 1
        
        if expensive_count > 10:
            issues.append(ValidationIssue(
                prim.GetPath(),
                f"Prim has many attributes with expensive data types ({expensive_count})",
                ValidationSeverity.WARNING,
                self.rule_id,
                "Consider using more efficient data types where possible"
            ))
        
        return issues


class EntityTypeValidationRule(ValidationRule):
    """Rule for validating entity type usage"""
    
    def __init__(self):
        super().__init__(
            "ENTITY_TYPE_001",
            "Entity types should have appropriate API schemas"
        )
    
    def validate(self, stage: Usd.Stage, prim: Usd.Prim) -> List[ValidationIssue]:
        issues = []
        
        # Check specific entity types
        if prim.IsA(Tf.Type.FindByName("SparkleEnemyCarrot")):
            return self._validate_enemy(prim)
        elif prim.IsA(Tf.Type.FindByName("SparklePlayer")):
            return self._validate_player(prim)
        elif prim.IsA(Tf.Type.FindByName("SparklePickup")):
            return self._validate_pickup(prim)
        
        return issues
    
    def _validate_enemy(self, prim: Usd.Prim) -> List[ValidationIssue]:
        """Validate enemy entity type"""
        issues = []
        
        # Enemies should have health, combat, and AI
        schemas = []
        prim.GetAppliedSchemas(schemas)
        
        expected_schemas = ["SparkleHealthAPI", "SparkleCombatAPI", "SparkleAIAPI"]
        for schema in expected_schemas:
            if schema not in schemas:
                issues.append(ValidationIssue(
                    prim.GetPath(),
                    f"Enemy is missing recommended schema: {schema}",
                    ValidationSeverity.WARNING,
                    self.rule_id,
                    f"Apply {schema} to this enemy entity"
                ))
        
        return issues
    
    def _validate_player(self, prim: Usd.Prim) -> List[ValidationIssue]:
        """Validate player entity type"""
        issues = []
        
        # Players should have health and movement
        schemas = []
        prim.GetAppliedSchemas(schemas)
        
        expected_schemas = ["SparkleHealthAPI", "SparkleMovementAPI"]
        for schema in expected_schemas:
            if schema not in schemas:
                issues.append(ValidationIssue(
                    prim.GetPath(),
                    f"Player is missing recommended schema: {schema}",
                    ValidationSeverity.WARNING,
                    self.rule_id,
                    f"Apply {schema} to this player entity"
                ))
        
        return issues
    
    def _validate_pickup(self, prim: Usd.Prim) -> List[ValidationIssue]:
        """Validate pickup entity type"""
        issues = []
        
        # Check for required pickup ID
        id_attr = prim.GetAttribute("sparkle:entity:id")
        if not id_attr:
            issues.append(ValidationIssue(
                prim.GetPath(),
                "Pickup is missing required 'sparkle:entity:id' attribute",
                ValidationSeverity.ERROR,
                self.rule_id,
                "Add 'sparkle:entity:id' attribute with unique ID"
            ))
        
        return issues


class SchemaValidator:
    """Main schema validator class"""
    
    def __init__(self):
        # Initialize rules
        self.rules = [
            NamespaceValidationRule(),
            HealthComponentValidationRule(),
            MovementComponentValidationRule(),
            AiComponentValidationRule(),
            PerformanceValidationRule(),
            EntityTypeValidationRule()
        ]
        
        # Initialize issue trackers
        self.all_issues = []
        self.issue_counts = {
            ValidationSeverity.ERROR: 0,
            ValidationSeverity.WARNING: 0,
            ValidationSeverity.INFO: 0
        }
    
    def validate_stage(self, stage: Usd.Stage) -> List[ValidationIssue]:
        """Validate all prims in a stage"""
        self.all_issues = []
        self.issue_counts = {
            ValidationSeverity.ERROR: 0,
            ValidationSeverity.WARNING: 0,
            ValidationSeverity.INFO: 0
        }
        
        # Validate each prim
        for prim in stage.Traverse():
            if prim.IsValid() and not prim.IsAbstract():
                self._validate_prim(stage, prim)
        
        return self.all_issues
    
    def _validate_prim(self, stage: Usd.Stage, prim: Usd.Prim):
        """Validate a single prim against all rules"""
        for rule in self.rules:
            issues = rule.validate(stage, prim)
            
            if issues:
                self.all_issues.extend(issues)
                
                # Update issue counts
                for issue in issues:
                    self.issue_counts[issue.severity] += 1
    
    def get_summary(self) -> str:
        """Get a summary of validation results"""
        return (
            f"Validation complete: "
            f"{self.issue_counts[ValidationSeverity.ERROR]} errors, "
            f"{self.issue_counts[ValidationSeverity.WARNING]} warnings, "
            f"{self.issue_counts[ValidationSeverity.INFO]} info"
        )
    
    def print_issues(self, severity_filter: Optional[ValidationSeverity] = None):
        """Print all validation issues, optionally filtered by severity"""
        for issue in self.all_issues:
            if severity_filter is None or issue.severity == severity_filter:
                print(issue)
    
    def has_errors(self) -> bool:
        """Check if validation found any errors"""
        return self.issue_counts[ValidationSeverity.ERROR] > 0
    
    def generate_report(self, output_path: str):
        """Generate a detailed validation report"""
        report = {
            "summary": {
                "errors": self.issue_counts[ValidationSeverity.ERROR],
                "warnings": self.issue_counts[ValidationSeverity.WARNING],
                "info": self.issue_counts[ValidationSeverity.INFO],
                "total": len(self.all_issues)
            },
            "issues": []
        }
        
        # Add issues to report
        for issue in self.all_issues:
            report["issues"].append({
                "path": str(issue.path),
                "message": issue.message,
                "severity": str(issue.severity.name),
                "rule_id": issue.rule_id,
                "fix_description": issue.fix_description
            })
        
        # Write report
        with open(output_path, 'w') as f:
            json.dump(report, f, indent=2)


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description="Validate USD game schemas")
    parser.add_argument("usd_file", help="USD file to validate")
    parser.add_argument("--report", help="Output detailed report to file")
    parser.add_argument("--errors-only", action="store_true", help="Show only errors")
    args = parser.parse_args()
    
    # Check if file exists
    if not os.path.exists(args.usd_file):
        print(f"Error: File not found: {args.usd_file}")
        return 1
    
    # Open USD stage
    stage = Usd.Stage.Open(args.usd_file)
    if not stage:
        print(f"Error: Failed to open USD stage: {args.usd_file}")
        return 1
    
    # Create validator and validate stage
    validator = SchemaValidator()
    validator.validate_stage(stage)
    
    # Print summary
    print(validator.get_summary())
    
    # Print issues
    severity_filter = ValidationSeverity.ERROR if args.errors_only else None
    validator.print_issues(severity_filter)
    
    # Generate report if requested
    if args.report:
        validator.generate_report(args.report)
        print(f"Detailed report written to: {args.report}")
    
    # Return error code if validation found errors
    return 1 if validator.has_errors() else 0


if __name__ == "__main__":
    sys.exit(main())
