#!/usr/bin/env python
"""
schema_editor.py

A GUI-based schema editor for creating and modifying USD schemas for game development.
This tool provides a user-friendly interface for working with codeless schemas without 
needing to write USD syntax directly.
"""

import sys
import os
from PySide2 import QtWidgets, QtCore, QtGui
from pxr import Usd, UsdGeom, Sdf, Tf

class SchemaPropertyWidget(QtWidgets.QWidget):
    """Widget for editing a single schema property"""
    
    propertyChanged = QtCore.Signal()
    
    def __init__(self, propertyName, propertyType, defaultValue=None, parent=None):
        super().__init__(parent)
        
        # Create layout
        layout = QtWidgets.QHBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)
        self.setLayout(layout)
        
        # Property name label
        self.nameLabel = QtWidgets.QLabel(propertyName)
        self.nameLabel.setMinimumWidth(150)
        layout.addWidget(self.nameLabel)
        
        # Property type label
        self.typeLabel = QtWidgets.QLabel(propertyType)
        self.typeLabel.setMinimumWidth(80)
        layout.addWidget(self.typeLabel)
        
        # Create appropriate editor based on type
        if propertyType == "float":
            self.editor = QtWidgets.QDoubleSpinBox()
            self.editor.setRange(-10000, 10000)
            self.editor.setSingleStep(0.1)
            if defaultValue is not None:
                self.editor.setValue(float(defaultValue))
            self.editor.valueChanged.connect(self._emitChange)
        elif propertyType == "int":
            self.editor = QtWidgets.QSpinBox()
            self.editor.setRange(-10000, 10000)
            if defaultValue is not None:
                self.editor.setValue(int(defaultValue))
            self.editor.valueChanged.connect(self._emitChange)
        elif propertyType == "token":
            self.editor = QtWidgets.QComboBox()
            # Add allowed token values
            if isinstance(defaultValue, list):
                self.editor.addItems(defaultValue)
            elif defaultValue is not None:
                self.editor.setCurrentText(str(defaultValue))
            self.editor.currentTextChanged.connect(self._emitChange)
        elif propertyType == "string":
            self.editor = QtWidgets.QLineEdit()
            if defaultValue is not None:
                self.editor.setText(str(defaultValue))
            self.editor.textChanged.connect(self._emitChange)
        elif propertyType == "bool":
            self.editor = QtWidgets.QCheckBox()
            if defaultValue is not None:
                self.editor.setChecked(bool(defaultValue))
            self.editor.stateChanged.connect(self._emitChange)
        else:
            self.editor = QtWidgets.QLineEdit()
            if defaultValue is not None:
                self.editor.setText(str(defaultValue))
            self.editor.textChanged.connect(self._emitChange)
        
        layout.addWidget(self.editor)
        
        # Delete button
        self.deleteButton = QtWidgets.QPushButton("X")
        self.deleteButton.setMaximumWidth(30)
        layout.addWidget(self.deleteButton)
    
    def _emitChange(self):
        """Emit the property changed signal"""
        self.propertyChanged.emit()
    
    def getValue(self):
        """Get the current value from the editor"""
        if isinstance(self.editor, QtWidgets.QDoubleSpinBox):
            return self.editor.value()
        elif isinstance(self.editor, QtWidgets.QSpinBox):
            return self.editor.value()
        elif isinstance(self.editor, QtWidgets.QComboBox):
            return self.editor.currentText()
        elif isinstance(self.editor, QtWidgets.QLineEdit):
            return self.editor.text()
        elif isinstance(self.editor, QtWidgets.QCheckBox):
            return self.editor.isChecked()
        return None
    
    def getPropertyName(self):
        """Get the property name"""
        return self.nameLabel.text()
    
    def getPropertyType(self):
        """Get the property type"""
        return self.typeLabel.text()


class AddPropertyDialog(QtWidgets.QDialog):
    """Dialog for adding a new property"""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        
        self.setWindowTitle("Add Property")
        self.setMinimumWidth(400)
        
        # Create layout
        layout = QtWidgets.QVBoxLayout()
        self.setLayout(layout)
        
        formLayout = QtWidgets.QFormLayout()
        layout.addLayout(formLayout)
        
        # Property name field
        self.nameEdit = QtWidgets.QLineEdit()
        formLayout.addRow("Property Name:", self.nameEdit)
        
        # Property namespace field
        self.namespaceEdit = QtWidgets.QLineEdit("sparkle:")
        formLayout.addRow("Namespace:", self.namespaceEdit)
        
        # Property type selector
        self.typeCombo = QtWidgets.QComboBox()
        self.typeCombo.addItems(["float", "int", "bool", "string", "token", "vector3f", "color3f", "matrix4d", "asset"])
        formLayout.addRow("Type:", self.typeCombo)
        
        # Default value field
        self.defaultEdit = QtWidgets.QLineEdit()
        formLayout.addRow("Default Value:", self.defaultEdit)
        
        # Token values field (for token type)
        self.tokenValuesEdit = QtWidgets.QLineEdit()
        self.tokenValuesEdit.setPlaceholderText("Value1,Value2,Value3")
        formLayout.addRow("Allowed Token Values:", self.tokenValuesEdit)
        
        # Documentation field
        self.docEdit = QtWidgets.QTextEdit()
        self.docEdit.setMaximumHeight(100)
        formLayout.addRow("Documentation:", self.docEdit)
        
        # Buttons
        buttonBox = QtWidgets.QDialogButtonBox(
            QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel
        )
        buttonBox.accepted.connect(self.accept)
        buttonBox.rejected.connect(self.reject)
        layout.addWidget(buttonBox)
        
        # Connect signals
        self.typeCombo.currentIndexChanged.connect(self._updateVisibility)
        
        # Initial visibility update
        self._updateVisibility()
    
    def _updateVisibility(self):
        """Update field visibility based on property type"""
        isToken = self.typeCombo.currentText() == "token"
        self.tokenValuesEdit.setEnabled(isToken)
    
    def getPropertyData(self):
        """Get the property data entered by the user"""
        fullName = self.namespaceEdit.text() + self.nameEdit.text()
        propType = self.typeCombo.currentText()
        defaultValue = self.defaultEdit.text()
        
        # Process token values
        tokenValues = None
        if propType == "token" and self.tokenValuesEdit.text():
            tokenValues = [x.strip() for x in self.tokenValuesEdit.text().split(",")]
        
        # Process documentation
        doc = self.docEdit.toPlainText()
        
        return {
            "name": fullName,
            "type": propType,
            "default": defaultValue,
            "tokenValues": tokenValues,
            "doc": doc
        }


class SchemaEditor(QtWidgets.QWidget):
    """Main schema editor widget"""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        
        # Set window title and size
        self.setWindowTitle("USD Game Schema Editor")
        self.resize(800, 600)
        
        # Create layout
        mainLayout = QtWidgets.QVBoxLayout()
        self.setLayout(mainLayout)
        
        # Schema type selection
        typeLayout = QtWidgets.QHBoxLayout()
        typeLabel = QtWidgets.QLabel("Schema Type:")
        self.typeCombo = QtWidgets.QComboBox()
        self.typeCombo.addItems([
            "Entity Schema (IsA)",
            "API Schema (Single-Apply)",
            "API Schema (Multiple-Apply)"
        ])
        self.typeCombo.currentIndexChanged.connect(self._updateSchemaTypeUI)
        
        typeLayout.addWidget(typeLabel)
        typeLayout.addWidget(self.typeCombo)
        mainLayout.addLayout(typeLayout)
        
        # Schema name fields
        nameLayout = QtWidgets.QHBoxLayout()
        nameLabel = QtWidgets.QLabel("Schema Name:")
        self.nameEdit = QtWidgets.QLineEdit()
        
        nameLayout.addWidget(nameLabel)
        nameLayout.addWidget(self.nameEdit)
        mainLayout.addLayout(nameLayout)
        
        # Schema parent fields
        parentLayout = QtWidgets.QHBoxLayout()
        parentLabel = QtWidgets.QLabel("Parent Schema:")
        self.parentCombo = QtWidgets.QComboBox()
        
        parentLayout.addWidget(parentLabel)
        parentLayout.addWidget(self.parentCombo)
        mainLayout.addLayout(parentLayout)
        
        # Library fields
        libraryLayout = QtWidgets.QHBoxLayout()
        libraryLabel = QtWidgets.QLabel("Library Name:")
        self.libraryEdit = QtWidgets.QLineEdit("sparkleGame")
        
        libraryLayout.addWidget(libraryLabel)
        libraryLayout.addWidget(self.libraryEdit)
        mainLayout.addLayout(libraryLayout)
        
        # Documentation fields
        docLabel = QtWidgets.QLabel("Documentation:")
        self.docEdit = QtWidgets.QTextEdit()
        
        mainLayout.addWidget(docLabel)
        mainLayout.addWidget(self.docEdit)
        
        # API Schema specific fields
        self.apiSchemaGroup = QtWidgets.QGroupBox("API Schema Settings")
        apiSchemaLayout = QtWidgets.QFormLayout()
        self.apiSchemaGroup.setLayout(apiSchemaLayout)
        
        self.namespaceEdit = QtWidgets.QLineEdit("sparkle")
        apiSchemaLayout.addRow("Property Namespace:", self.namespaceEdit)
        
        mainLayout.addWidget(self.apiSchemaGroup)
        
        # Splitter for properties and preview
        splitter = QtWidgets.QSplitter(QtCore.Qt.Vertical)
        mainLayout.addWidget(splitter, 1)
        
        # Properties group
        propsGroupBox = QtWidgets.QGroupBox("Properties")
        propsLayout = QtWidgets.QVBoxLayout()
        propsGroupBox.setLayout(propsLayout)
        
        # Properties toolbar
        propToolbar = QtWidgets.QHBoxLayout()
        addPropButton = QtWidgets.QPushButton("Add Property")
        addPropButton.clicked.connect(self._addProperty)
        propToolbar.addWidget(addPropButton)
        propToolbar.addStretch()
        
        propsLayout.addLayout(propToolbar)
        
        # Properties list
        self.propsScroll = QtWidgets.QScrollArea()
        self.propsScroll.setWidgetResizable(True)
        self.propsWidget = QtWidgets.QWidget()
        self.propsLayout = QtWidgets.QVBoxLayout(self.propsWidget)
        self.propsLayout.addStretch()
        self.propsScroll.setWidget(self.propsWidget)
        
        propsLayout.addWidget(self.propsScroll, 1)
        splitter.addWidget(propsGroupBox)
        
        # Preview group
        previewGroupBox = QtWidgets.QGroupBox("USD Preview")
        previewLayout = QtWidgets.QVBoxLayout()
        previewGroupBox.setLayout(previewLayout)
        
        self.previewText = QtWidgets.QTextEdit()
        self.previewText.setReadOnly(True)
        self.previewText.setFont(QtGui.QFont("Courier", 10))
        previewLayout.addWidget(self.previewText)
        
        splitter.addWidget(previewGroupBox)
        
        # Buttons
        buttonLayout = QtWidgets.QHBoxLayout()
        self.saveButton = QtWidgets.QPushButton("Save Schema")
        self.saveButton.clicked.connect(self._saveSchema)
        buttonLayout.addWidget(self.saveButton)
        
        self.clearButton = QtWidgets.QPushButton("Clear")
        self.clearButton.clicked.connect(self._clearForm)
        buttonLayout.addWidget(self.clearButton)
        
        mainLayout.addLayout(buttonLayout)
        
        # Initialize UI
        self._updateSchemaTypeUI()
        self._populateParentSchemas()
        
        # Update preview when anything changes
        self.typeCombo.currentIndexChanged.connect(self._updatePreview)
        self.nameEdit.textChanged.connect(self._updatePreview)
        self.parentCombo.currentTextChanged.connect(self._updatePreview)
        self.libraryEdit.textChanged.connect(self._updatePreview)
        self.docEdit.textChanged.connect(self._updatePreview)
        self.namespaceEdit.textChanged.connect(self._updatePreview)
    
    def _updateSchemaTypeUI(self):
        """Update UI based on selected schema type"""
        schemaType = self.typeCombo.currentIndex()
        
        # Update parent schema options
        self._populateParentSchemas()
        
        # Update API schema settings visibility
        self.apiSchemaGroup.setVisible(schemaType > 0)
        
        # Update preview
        self._updatePreview()
    
    def _populateParentSchemas(self):
        """Populate the parent schema dropdown based on schema type"""
        self.parentCombo.clear()
        
        schemaType = self.typeCombo.currentIndex()
        if schemaType == 0:  # Entity Schema
            self.parentCombo.addItems(["Typed", "GeomXformable", "GeomMesh", "GeomBasisCurves"])
        else:  # API Schema
            self.parentCombo.addItems(["APISchemaBase"])
    
    def _addProperty(self):
        """Add a new property to the schema"""
        dialog = AddPropertyDialog(self)
        if dialog.exec_() == QtWidgets.QDialog.Accepted:
            propData = dialog.getPropertyData()
            
            # Create property widget
            propWidget = SchemaPropertyWidget(
                propData["name"], 
                propData["type"], 
                propData["default"] if propData["default"] else None
            )
            propWidget.propertyChanged.connect(self._updatePreview)
            propWidget.deleteButton.clicked.connect(
                lambda: self._removeProperty(propWidget)
            )
            
            # Add to layout (before the stretch)
            self.propsLayout.insertWidget(self.propsLayout.count() - 1, propWidget)
            
            # Update preview
            self._updatePreview()
    
    def _removeProperty(self, widget):
        """Remove a property widget"""
        self.propsLayout.removeWidget(widget)
        widget.deleteLater()
        self._updatePreview()
    
    def _updatePreview(self):
        """Update the USD preview text"""
        preview = self._generateUsdPreview()
        self.previewText.setText(preview)
    
    def _generateUsdPreview(self):
        """Generate USD preview text"""
        schemaType = self.typeCombo.currentIndex()
        schemaName = self.nameEdit.text()
        parentSchema = self.parentCombo.currentText()
        libraryName = self.libraryEdit.text()
        doc = self.docEdit.toPlainText()
        
        # Start with USDA header
        preview = "#usda 1.0\n\n"
        
        # Add GLOBAL section
        preview += 'over "GLOBAL" (\n'
        preview += '    customData = {\n'
        preview += f'        string libraryName = "{libraryName}"\n'
        preview += '        string libraryPath = "./"\n'
        preview += f'        string libraryPrefix = "{libraryName}"\n'
        preview += '        bool skipCodeGeneration = true\n'
        preview += '    }\n'
        preview += ') {\n'
        preview += '}\n\n'
        
        # Add schema class
        if schemaType == 0:  # Entity Schema
            # IsA schema
            preview += f'class "{schemaName}" (\n'
            preview += f'    inherits = </{parentSchema}>\n'
            if doc:
                preview += f'    doc = """{doc}"""\n'
            preview += ') {\n'
        else:  # API Schema
            # API Schema
            apiType = "singleApply" if schemaType == 1 else "multipleApply"
            preview += f'class "{schemaName}API" (\n'
            preview += f'    inherits = </{parentSchema}>\n'
            preview += '    customData = {\n'
            preview += f'        token apiSchemaType = "{apiType}"\n'
            
            # Add namespace prefix for multiple-apply API
            if schemaType == 2:
                namespace = self.namespaceEdit.text()
                preview += f'        token propertyNamespacePrefix = "{namespace}"\n'
            
            preview += '    }\n'
            
            if doc:
                preview += f'    doc = """{doc}"""\n'
            
            preview += ') {\n'
        
        # Add properties
        for i in range(self.propsLayout.count() - 1):  # Skip the stretch item
            widget = self.propsLayout.itemAt(i).widget()
            if isinstance(widget, SchemaPropertyWidget):
                propName = widget.getPropertyName()
                propType = widget.getPropertyType()
                propValue = widget.getValue()
                
                # Format property value based on type
                valueStr = ""
                if propType == "float":
                    valueStr = str(propValue)
                elif propType == "int":
                    valueStr = str(propValue)
                elif propType == "bool":
                    valueStr = "true" if propValue else "false"
                elif propType == "token":
                    valueStr = f'"{propValue}"'
                else:
                    valueStr = f'"{propValue}"'
                
                preview += f'    {propType} {propName} = {valueStr}\n'
        
        # Close class
        preview += '}\n'
        
        return preview
    
    def _saveSchema(self):
        """Save the schema to a file"""
        fileName, _ = QtWidgets.QFileDialog.getSaveFileName(
            self, "Save Schema", "", "USD Files (*.usda)"
        )
        
        if fileName:
            if not fileName.endswith(".usda"):
                fileName += ".usda"
            
            with open(fileName, "w") as f:
                f.write(self._generateUsdPreview())
            
            QtWidgets.QMessageBox.information(
                self, "Schema Saved", f"Schema saved to {fileName}"
            )
    
    def _clearForm(self):
        """Clear the form"""
        self.nameEdit.clear()
        self.docEdit.clear()
        
        # Clear properties
        for i in reversed(range(self.propsLayout.count() - 1)):  # Skip the stretch item
            widget = self.propsLayout.itemAt(i).widget()
            if widget:
                widget.deleteLater()
        
        # Reset schema type
        self.typeCombo.setCurrentIndex(0)
        
        # Update preview
        self._updatePreview()


def main():
    """Main application entry point"""
    app = QtWidgets.QApplication(sys.argv)
    
    # Set application style
    app.setStyle("Fusion")
    
    # Create and show the editor
    editor = SchemaEditor()
    editor.show()
    
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
