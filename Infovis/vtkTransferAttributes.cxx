/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkTransferAttributes.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/*-------------------------------------------------------------------------
  Copyright 2008 Sandia Corporation.
  Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
  the U.S. Government retains certain rights in this software.
-------------------------------------------------------------------------*/
#include "vtkTransferAttributes.h"

#include "vtkDataObject.h"
#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkEdgeListIterator.h"
#include "vtkFloatArray.h"
#include "vtkGraph.h"
#include "vtkIdTypeArray.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkSmartPointer.h"
#include "vtkStdString.h"
#include "vtkTree.h"
#include "vtkVariantArray.h"
#include "vtkIntArray.h"
#include "vtkDataSet.h" 
#include "vtkTable.h"

#include <vtksys/stl/map>
using vtksys_stl::map;

//---------------------------------------------------------------------------
template <typename T>
vtkVariant vtkGetValue(T* arr, vtkIdType index)
{
  return vtkVariant(arr[index]);
}

//---------------------------------------------------------------------------
static vtkVariant vtkGetVariantValue(vtkAbstractArray* arr, vtkIdType i)
{
  vtkVariant val;
  switch(arr->GetDataType())
    {
    vtkExtraExtendedTemplateMacro(val = vtkGetValue(
      static_cast<VTK_TT*>(arr->GetVoidPointer(0)), i));
    }
  return val;
}

vtkCxxRevisionMacro(vtkTransferAttributes, "1.1");
vtkStandardNewMacro(vtkTransferAttributes);

vtkTransferAttributes::vtkTransferAttributes()
{
  this->SetNumberOfInputPorts(2);
  this->DirectMapping = false;
  this->DefaultValue = 1;
  this->SourceArrayName = 0;
  this->TargetArrayName = 0;
}

vtkTransferAttributes::~vtkTransferAttributes()
{
  this->SetSourceArrayName(0);
  this->SetTargetArrayName(0);
}

int vtkTransferAttributes::FillInputPortInformation(int port, vtkInformation* info)
{
  if( port == 0 )
  {
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataObject");
    return 1;
  }
  else if (port == 1)
  {
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataObject");
    return 1;
  }
  return 0;
}

int vtkTransferAttributes::RequestData(
  vtkInformation *vtkNotUsed(request),
  vtkInformationVector **inputVector,
  vtkInformationVector *outputVector)
{
  // get the info objects
  vtkInformation *targetInfo = inputVector[0]->GetInformationObject(0);
  vtkInformation *sourceInfo = inputVector[1]->GetInformationObject(0);
  vtkInformation *outInfo = outputVector->GetInformationObject(0);

  vtkDataObject* sourceInput = sourceInfo->Get(vtkDataObject::DATA_OBJECT());
  vtkDataObject* targetInput = targetInfo->Get(vtkDataObject::DATA_OBJECT());
  vtkDataObject* output = outInfo->Get(vtkDataObject::DATA_OBJECT());
  
  output->ShallowCopy(targetInput);
  
  // get the input and ouptut
  vtkDataSetAttributes* dsa_source = 0;
  if (vtkDataSet::SafeDownCast(sourceInput) && 
      this->SourceFieldType == vtkDataObject::FIELD_ASSOCIATION_POINTS)
  {
    dsa_source = vtkDataSet::SafeDownCast(sourceInput)->GetPointData();
  }
  else if (vtkDataSet::SafeDownCast(sourceInput) &&
           this->SourceFieldType == vtkDataObject::FIELD_ASSOCIATION_CELLS)
  {
    dsa_source = vtkDataSet::SafeDownCast(sourceInput)->GetCellData();
  }
  else if (vtkGraph::SafeDownCast(sourceInput) &&
           this->SourceFieldType == vtkDataObject::FIELD_ASSOCIATION_VERTICES)
  {
    dsa_source = vtkGraph::SafeDownCast(sourceInput)->GetVertexData();
  }
  else if (vtkGraph::SafeDownCast(sourceInput) && 
           this->SourceFieldType == vtkDataObject::FIELD_ASSOCIATION_EDGES)
  {
    dsa_source = vtkGraph::SafeDownCast(sourceInput)->GetEdgeData();
  }
  else if (vtkTable::SafeDownCast(sourceInput) &&
           this->SourceFieldType == vtkDataObject::FIELD_ASSOCIATION_ROWS)
  {
    dsa_source = vtkTable::SafeDownCast(sourceInput)->GetRowData();
  }
  else
  {
      // ERROR 
    vtkErrorMacro( "Input type must be specified as a dataset, graph or table." );
    return 0;
  }

  vtkDataSetAttributes* dsa_target = 0;
  vtkDataSetAttributes* dsa_out = 0;
  if (vtkDataSet::SafeDownCast(targetInput) && 
      this->TargetFieldType == vtkDataObject::FIELD_ASSOCIATION_POINTS)
  {
    dsa_target = vtkDataSet::SafeDownCast(targetInput)->GetPointData();
    dsa_out = vtkDataSet::SafeDownCast(output)->GetPointData();
  }
  else if (vtkDataSet::SafeDownCast(targetInput) &&
           this->TargetFieldType == vtkDataObject::FIELD_ASSOCIATION_CELLS)
  {
    dsa_target = vtkDataSet::SafeDownCast(targetInput)->GetCellData();
    dsa_out = vtkDataSet::SafeDownCast(output)->GetCellData();
  }
  else if (vtkGraph::SafeDownCast(targetInput) &&
           this->TargetFieldType == vtkDataObject::FIELD_ASSOCIATION_VERTICES)
  {
    dsa_target = vtkGraph::SafeDownCast(targetInput)->GetVertexData();
    dsa_out = vtkGraph::SafeDownCast(output)->GetVertexData();
  }
  else if (vtkGraph::SafeDownCast(targetInput) && 
           this->TargetFieldType == vtkDataObject::FIELD_ASSOCIATION_EDGES)
  {
    dsa_target = vtkGraph::SafeDownCast(targetInput)->GetEdgeData();
    dsa_out = vtkGraph::SafeDownCast(output)->GetEdgeData();
  }
  else if (vtkTable::SafeDownCast(targetInput) &&
           this->TargetFieldType == vtkDataObject::FIELD_ASSOCIATION_ROWS)
  {
    dsa_target = vtkTable::SafeDownCast(targetInput)->GetRowData();
    dsa_out = vtkTable::SafeDownCast(output)->GetRowData();
  }
  else
  {
      // ERROR 
    vtkErrorMacro( "Input type must be specified as a dataset, graph or table." );
    return 0;
  }

  if( this->SourceArrayName == 0 || this->TargetArrayName == 0 )
  {
    vtkErrorMacro( "Must specify source and target array names for the transfer." );
    return 0;
  }

  vtkAbstractArray* sourceIdArray = dsa_source->GetPedigreeIds();
  vtkAbstractArray* targetIdArray = dsa_target->GetPedigreeIds();

    // Check for valid pedigree id arrays.
  if (!sourceIdArray)
  {
    vtkErrorMacro("SourceInput pedigree id array not found.");
    return 0;
  }
  if (!targetIdArray)
  {
    vtkErrorMacro("TargetInput pedigree id array not found.");
    return 0;
  }
  
  // Create a map from sourceInput indices to targetInput indices
  // If we are using DirectMapping this is trivial
  // we just create an identity map
  int i;
  map<vtkIdType, vtkIdType> sourceIndexToTargetIndex;
  if (this->DirectMapping)
  {
    if (sourceIdArray->GetNumberOfTuples() > targetIdArray->GetNumberOfTuples())
    {
      vtkErrorMacro("Cannot have more sourceInput tuples than targetInput values using direct mapping.");
      return 0;
    }
      // Create identity map.
    for( i = 0; i < sourceIdArray->GetNumberOfTuples(); i++)
    {
      sourceIndexToTargetIndex[i] = i;
    }
  }
    
  // Okay if we do not have direct mapping then we need
  // to do some templated madness to go from an arbitrary
  // type to a nice vtkIdType to vtkIdType mapping
  if (!this->DirectMapping)
  {
    map<vtkVariant,vtkIdType,vtkVariantLessThan> sourceInputIdMap;
    
    // Create a map from sourceInput id to sourceInput index
    for( i=0; i<sourceIdArray->GetNumberOfTuples(); i++)
    {
      sourceInputIdMap[vtkGetVariantValue(sourceIdArray,i)] = i;
    }
      
    // Now create the map from sourceInput index to targetInput index
    for( i=0; i < targetIdArray->GetNumberOfTuples(); i++)
    {
      vtkVariant id = vtkGetVariantValue(targetIdArray,i);
      if (sourceInputIdMap.count(id))
      {
        sourceIndexToTargetIndex[sourceInputIdMap[id]] = i;
      }
    }
  }
  
  vtkAbstractArray *sourceArray = dsa_source->GetAbstractArray( this->SourceArrayName );
  vtkAbstractArray *targetArray = vtkAbstractArray::CreateArray(sourceArray->GetDataType());
  targetArray->SetName( this->TargetArrayName );

  targetArray->SetNumberOfComponents(sourceArray->GetNumberOfComponents());
  targetArray->SetNumberOfTuples(targetIdArray->GetNumberOfTuples());

  for( i = 0; i < targetArray->GetNumberOfTuples(); i++)
  {
    targetArray->InsertVariantValue(i, this->DefaultValue);
  }
  
  for( i = 0; i < sourceArray->GetNumberOfTuples(); i++)
  {
    if( sourceArray->GetVariantValue(i) < 0 )
    {
      cout << sourceIndexToTargetIndex[i] << " " 
           << sourceArray->GetVariantValue(i).ToString() << " " 
           << sourceArray->GetNumberOfTuples() << " " 
           << sourceIdArray->GetNumberOfTuples() << " " 
           << i << endl;
      
      vtkErrorMacro( "Bad value..." );
      continue;
    }
    targetArray->SetTuple(sourceIndexToTargetIndex[i], i, sourceArray);
  }

  dsa_out->AddArray(targetArray);
  targetArray->Delete();

  return 1;
}

vtkVariant vtkTransferAttributes::GetDefaultValue()
{
  return this->DefaultValue;
}

void vtkTransferAttributes::SetDefaultValue(vtkVariant value)
{
  this->DefaultValue = value;
}

void vtkTransferAttributes::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
  os << indent << "DirectMapping: " << this->DirectMapping << endl;
  os << indent << "DefaultValue: " << this->DefaultValue.ToString() << endl;
  os << indent << "SourceArrayName: " << (this->SourceArrayName ? this->SourceArrayName : "(none)") << endl;
  os << indent << "TargetArrayName: " << (this->TargetArrayName ? this->TargetArrayName : "(none)") << endl;
  os << indent << "SourceFieldType: " << this->SourceFieldType << endl;
  os << indent << "TargetFieldType: " << this->TargetFieldType << endl;
}

