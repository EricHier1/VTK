/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkArrayListTemplate.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See LICENSE file for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkArrayListTemplate
 * @brief   thread-safe and efficient data attribute processing
 *
 *
 * vtkArrayListTemplate supplements the vtkDataSetAttributes class to provide
 * threaded processing of data arrays. It is also more efficient for certain
 * interpolation operations. The expectation is that it will be replaced one
 * day once vtkPointData, vtkCellData, vtkDataSetAttributes, and vtkFieldData
 * properly support multithreading and/or are redesigned. Note that this
 * implementation does not support incremental operations (like InsertNext()).
 *
 * Generally the way this helper class is used is to first invoke
 * vtkDataSetAttributes::CopyInterpolate() or InterpolateAllocate() which
 * performs the initial magic of constructing input and output arrays. Then
 * the input attributes, and output attributes, are passed to initialize the
 * internal structures via the AddArrays() method. Essentially these internal
 * structures are templated pairs of arrays of the same type, which can be
 * efficiently accessed and assigned. The operations on these array pairs
 * (e.g., interpolation) occur using a typeless, virtual dispatch base class.
 *
 * @warning
 * vtkDataSetAttributes is not in general thread safe due to the use of its
 * vtkFieldData::BasicIterator RequiredArrays data member. This class augments
 * vtkDataSetAttributes for thread safety.
 *
 * @sa
 * vtkFieldData vtkDataSetAttributes vtkPointData vtkCellData
 */

#ifndef vtkArrayListTemplate_h
#define vtkArrayListTemplate_h

#include "vtkAbstractArray.h"
#include "vtkDataSetAttributes.h"
#include "vtkSmartPointer.h"
#include "vtkStdString.h"

#include <algorithm>
#include <vector>

// Create a generic class supporting virtual dispatch to type-specific
// subclasses.
struct BaseArrayPair
{
  vtkIdType Num;
  int NumComp;
  vtkSmartPointer<vtkAbstractArray> OutputArray;

  BaseArrayPair(vtkIdType num, int numComp, vtkAbstractArray* outArray)
    : Num(num)
    , NumComp(numComp)
    , OutputArray(outArray)
  {
  }
  virtual ~BaseArrayPair() = default;

  virtual void Copy(vtkIdType inId, vtkIdType outId) = 0;
  virtual void Interpolate(
    int numWeights, const vtkIdType* ids, const double* weights, vtkIdType outId) = 0;
  virtual void InterpolateOutput(
    int numWeights, const vtkIdType* ids, const double* weights, vtkIdType outId) = 0;
  virtual void Average(int numPts, const vtkIdType* ids, vtkIdType outId) = 0;
  virtual void WeightedAverage(
    int numPts, const vtkIdType* ids, const double* weights, vtkIdType outId) = 0;
  virtual void InterpolateEdge(vtkIdType v0, vtkIdType v1, double t, vtkIdType outId) = 0;
  virtual void AssignNullValue(vtkIdType outId) = 0;
  virtual void Realloc(vtkIdType sze) = 0;
};

// Type specific interpolation on a matched pair of data arrays
template <typename T>
struct ArrayPair : public BaseArrayPair
{
  T* Input;
  T* Output;
  T NullValue;

  ArrayPair(T* in, T* out, vtkIdType num, int numComp, vtkAbstractArray* outArray, T null)
    : BaseArrayPair(num, numComp, outArray)
    , Input(in)
    , Output(out)
    , NullValue(null)
  {
  }
  ~ArrayPair() override = default; // calm down some finicky compilers

  void Copy(vtkIdType inId, vtkIdType outId) override
  {
    for (int j = 0; j < this->NumComp; ++j)
    {
      this->Output[outId * this->NumComp + j] = this->Input[inId * this->NumComp + j];
    }
  }

  void Interpolate(
    int numWeights, const vtkIdType* ids, const double* weights, vtkIdType outId) override
  {
    for (int j = 0; j < this->NumComp; ++j)
    {
      double v = 0.0;
      for (int i = 0; i < numWeights; ++i)
      {
        v += weights[i] * static_cast<double>(this->Input[ids[i] * this->NumComp + j]);
      }
      this->Output[outId * this->NumComp + j] = static_cast<T>(v);
    }
  }

  void InterpolateOutput(
    int numWeights, const vtkIdType* ids, const double* weights, vtkIdType outId) override
  {
    for (int j = 0; j < this->NumComp; ++j)
    {
      double v = 0.0;
      for (int i = 0; i < numWeights; ++i)
      {
        v += weights[i] * static_cast<double>(this->Output[ids[i] * this->NumComp + j]);
      }
      this->Output[outId * this->NumComp + j] = static_cast<T>(v);
    }
  }

  void Average(int numPts, const vtkIdType* ids, vtkIdType outId) override
  {
    for (int j = 0; j < this->NumComp; ++j)
    {
      double v = 0.0;
      for (int i = 0; i < numPts; ++i)
      {
        v += static_cast<double>(this->Input[ids[i] * this->NumComp + j]);
      }
      v /= static_cast<double>(numPts);
      this->Output[outId * this->NumComp + j] = static_cast<T>(v);
    }
  }

  void WeightedAverage(
    int numPts, const vtkIdType* ids, const double* weights, vtkIdType outId) override
  {
    for (int j = 0; j < this->NumComp; ++j)
    {
      double v = 0.0;
      for (int i = 0; i < numPts; ++i)
      {
        v += (weights[i] * static_cast<double>(this->Input[ids[i] * this->NumComp + j]));
      }
      this->Output[outId * this->NumComp + j] = static_cast<T>(v);
    }
  }

  void InterpolateEdge(vtkIdType v0, vtkIdType v1, double t, vtkIdType outId) override
  {
    double v;
    vtkIdType numComp = this->NumComp;
    for (int j = 0; j < numComp; ++j)
    {
      v = this->Input[v0 * numComp + j] +
        t * (this->Input[v1 * numComp + j] - this->Input[v0 * numComp + j]);
      this->Output[outId * numComp + j] = static_cast<T>(v);
    }
  }

  void AssignNullValue(vtkIdType outId) override
  {
    for (int j = 0; j < this->NumComp; ++j)
    {
      this->Output[outId * this->NumComp + j] = this->NullValue;
    }
  }

  void Realloc(vtkIdType sze) override
  {
    this->OutputArray->Resize(sze);
    this->OutputArray->SetNumberOfTuples(sze);
    this->Output = static_cast<T*>(this->OutputArray->GetVoidPointer(0));
  }
};

// Type specific interpolation on a pair of data arrays with different types, where the
// output type is expected to be a real type (i.e., float or double).
template <typename TInput, typename TOutput>
struct RealArrayPair : public BaseArrayPair
{
  TInput* Input;
  TOutput* Output;
  TOutput NullValue;

  RealArrayPair(
    TInput* in, TOutput* out, vtkIdType num, int numComp, vtkAbstractArray* outArray, TOutput null)
    : BaseArrayPair(num, numComp, outArray)
    , Input(in)
    , Output(out)
    , NullValue(null)
  {
  }
  ~RealArrayPair() override = default; // calm down some finicky compilers

  void Copy(vtkIdType inId, vtkIdType outId) override
  {
    for (int j = 0; j < this->NumComp; ++j)
    {
      this->Output[outId * this->NumComp + j] =
        static_cast<TOutput>(this->Input[inId * this->NumComp + j]);
    }
  }

  void Interpolate(
    int numWeights, const vtkIdType* ids, const double* weights, vtkIdType outId) override
  {
    for (int j = 0; j < this->NumComp; ++j)
    {
      double v = 0.0;
      for (int i = 0; i < numWeights; ++i)
      {
        v += weights[i] * static_cast<double>(this->Input[ids[i] * this->NumComp + j]);
      }
      this->Output[outId * this->NumComp + j] = static_cast<TOutput>(v);
    }
  }

  void InterpolateOutput(
    int numWeights, const vtkIdType* ids, const double* weights, vtkIdType outId) override
  {
    for (int j = 0; j < this->NumComp; ++j)
    {
      double v = 0.0;
      for (int i = 0; i < numWeights; ++i)
      {
        v += weights[i] * static_cast<double>(this->Output[ids[i] * this->NumComp + j]);
      }
      this->Output[outId * this->NumComp + j] = static_cast<TOutput>(v);
    }
  }

  void Average(int numPts, const vtkIdType* ids, vtkIdType outId) override
  {
    for (int j = 0; j < this->NumComp; ++j)
    {
      double v = 0.0;
      for (int i = 0; i < numPts; ++i)
      {
        v += static_cast<double>(this->Input[ids[i] * this->NumComp + j]);
      }
      v /= static_cast<double>(numPts);
      this->Output[outId * this->NumComp + j] = static_cast<TOutput>(v);
    }
  }

  void WeightedAverage(
    int numPts, const vtkIdType* ids, const double* weights, vtkIdType outId) override
  {
    for (int j = 0; j < this->NumComp; ++j)
    {
      double v = 0.0;
      for (int i = 0; i < numPts; ++i)
      {
        v += (weights[i] * static_cast<double>(this->Input[ids[i] * this->NumComp + j]));
      }
      this->Output[outId * this->NumComp + j] = static_cast<TOutput>(v);
    }
  }

  void InterpolateEdge(vtkIdType v0, vtkIdType v1, double t, vtkIdType outId) override
  {
    double v;
    vtkIdType numComp = this->NumComp;
    for (int j = 0; j < numComp; ++j)
    {
      v = this->Input[v0 * numComp + j] +
        t * (this->Input[v1 * numComp + j] - this->Input[v0 * numComp + j]);
      this->Output[outId * numComp + j] = static_cast<TOutput>(v);
    }
  }

  void AssignNullValue(vtkIdType outId) override
  {
    for (int j = 0; j < this->NumComp; ++j)
    {
      this->Output[outId * this->NumComp + j] = this->NullValue;
    }
  }

  void Realloc(vtkIdType sze) override
  {
    this->OutputArray->Resize(sze);
    this->OutputArray->SetNumberOfTuples(sze);
    this->Output = static_cast<TOutput*>(this->OutputArray->GetVoidPointer(0));
  }
};

// Forward declarations. This makes working with vtkTemplateMacro easier.
struct ArrayList;

template <typename T>
void CreateArrayPair(
  ArrayList* list, T* inData, T* outData, vtkIdType numTuples, int numComp, T nullValue);

// A list of the arrays to interpolate, and a method to invoke interpolation on the list
struct ArrayList
{
  // The list of arrays, and the arrays not to process
  std::vector<BaseArrayPair*> Arrays;
  std::vector<vtkAbstractArray*> ExcludedArrays;

  // Add the arrays to interpolate here (from attribute data). Note that this method is
  // not thread-safe due to its use of vtkDataSetAttributes.
  void AddArrays(vtkIdType numOutPts, vtkDataSetAttributes* inPD, vtkDataSetAttributes* outPD,
    double nullValue = 0.0, vtkTypeBool promote = true);

  // Add an array that interpolates from its own attribute values
  void AddSelfInterpolatingArrays(
    vtkIdType numOutPts, vtkDataSetAttributes* attr, double nullValue = 0.0);

  // Add a pair of arrays (manual insertion). Returns the output array created,
  // if any. No array may be created if \c inArray was previously marked as
  // excluded using ExcludeArray().
  vtkAbstractArray* AddArrayPair(vtkIdType numTuples, vtkAbstractArray* inArray,
    vtkStdString& outArrayName, double nullValue, vtkTypeBool promote);

  // Any array excluded here is not added by AddArrays() or AddArrayPair, hence not
  // processed. Also check whether an array is excluded.
  void ExcludeArray(vtkAbstractArray* da);
  vtkTypeBool IsExcluded(vtkAbstractArray* da);

  // Loop over the array pairs and copy data from one to another. This (and the following methods)
  // can be used within threads.
  void Copy(vtkIdType inId, vtkIdType outId)
  {
    for (auto& array : this->Arrays)
    {
      array->Copy(inId, outId);
    }
  }

  // Loop over the arrays and have them interpolate themselves
  void Interpolate(int numWeights, const vtkIdType* ids, const double* weights, vtkIdType outId)
  {
    for (auto& array : this->Arrays)
    {
      array->Interpolate(numWeights, ids, weights, outId);
    }
  }

  // Loop over the arrays and have them interpolate themselves based on the output arrays
  void InterpolateOutput(
    int numWeights, const vtkIdType* ids, const double* weights, vtkIdType outId)
  {
    for (auto& array : this->Arrays)
    {
      array->InterpolateOutput(numWeights, ids, weights, outId);
    }
  }

  // Loop over the arrays and have them averaged
  void Average(int numPts, const vtkIdType* ids, vtkIdType outId)
  {
    for (auto& array : this->Arrays)
    {
      array->Average(numPts, ids, outId);
    }
  }

  // Loop over the arrays and weighted average the attributes. The weights should sum to 1.0.
  void WeightedAverage(int numPts, const vtkIdType* ids, const double* weights, vtkIdType outId)
  {
    for (auto& array : this->Arrays)
    {
      array->WeightedAverage(numPts, ids, weights, outId);
    }
  }

  // Loop over the arrays perform edge interpolation
  void InterpolateEdge(vtkIdType v0, vtkIdType v1, double t, vtkIdType outId)
  {
    for (auto& array : this->Arrays)
    {
      array->InterpolateEdge(v0, v1, t, outId);
    }
  }

  // Loop over the arrays and assign the null value
  void AssignNullValue(vtkIdType outId)
  {
    for (auto& array : this->Arrays)
    {
      array->AssignNullValue(outId);
    }
  }

  // Extend (realloc) the arrays
  void Realloc(vtkIdType sze)
  {
    for (auto& array : this->Arrays)
    {
      array->Realloc(sze);
    }
  }

  // Only you can prevent memory leaks!
  ~ArrayList()
  {
    for (auto& array : this->Arrays)
    {
      delete array;
    }
  }

  // Return the number of arrays
  vtkIdType GetNumberOfArrays() { return static_cast<vtkIdType>(Arrays.size()); }
};

#include "vtkArrayListTemplate.txx"

#endif
// VTK-HeaderTest-Exclude: vtkArrayListTemplate.h
