#include "vtkSlicerVolumeResliceDriverLogic.h"
/*==============================================================================

  Program: 3D Slicer

  Portions (c) Copyright Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

// VolumeResliceDriver includes
#include "vtkSlicerVolumeResliceDriverLogic.h"

// MRML includes
#include "vtkMRMLLinearTransformNode.h"
#include "vtkMRMLMarkupsLineNode.h"
#include "vtkMRMLMarkupsPlaneNode.h"
#include "vtkMRMLScalarVolumeNode.h"
#include "vtkMRMLSliceNode.h"
#include "vtkMRMLAnnotationRulerNode.h"
#include "vtkMRMLScene.h"

// VTK includes
#include <vtkCollection.h>
#include <vtkCollectionIterator.h>
#include <vtkImageData.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkTransform.h>
#include <vtkObjectFactory.h>
#include <vtkMRMLMarkupsFiducialNode.h>


vtkStandardNewMacro(vtkSlicerVolumeResliceDriverLogic);



vtkSlicerVolumeResliceDriverLogic
::vtkSlicerVolumeResliceDriverLogic()
{
}



vtkSlicerVolumeResliceDriverLogic
::~vtkSlicerVolumeResliceDriverLogic()
{
  this->ClearObservedNodes();
}



void vtkSlicerVolumeResliceDriverLogic
::PrintSelf( ostream& os, vtkIndent indent )
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "Number of observed nodes: " << this->ObservedNodes.size() << std::endl;
  os << indent << "Observed nodes:";

  for ( unsigned int i = 0; i < this->ObservedNodes.size(); ++ i )
    {
    os << " " << this->ObservedNodes[ i ]->GetID();
    }

  os << std::endl;
}



void vtkSlicerVolumeResliceDriverLogic
::SetDriverForSlice( std::string nodeID, vtkMRMLSliceNode* sliceNode )
{
  vtkMRMLNode* node = this->GetMRMLScene()->GetNodeByID( nodeID );
  if ( node == NULL )
    {
    sliceNode->RemoveAttribute( VOLUMERESLICEDRIVER_DRIVER_ATTRIBUTE );
    return;
    }

  vtkMRMLTransformableNode* tnode = vtkMRMLTransformableNode::SafeDownCast( node );
  if ( tnode == NULL )
    {
    sliceNode->RemoveAttribute( VOLUMERESLICEDRIVER_DRIVER_ATTRIBUTE );
    return;
    }

  sliceNode->SetAttribute( VOLUMERESLICEDRIVER_DRIVER_ATTRIBUTE, nodeID.c_str() );
  this->AddObservedNode( tnode );

  this->UpdateSliceIfObserved( sliceNode );
}


void vtkSlicerVolumeResliceDriverLogic
::SetModeForSlice( int mode, vtkMRMLSliceNode* sliceNode )
{
  if ( sliceNode == NULL )
    {
    return;
    }

  std::stringstream modeSS;
  modeSS << mode;
  sliceNode->SetAttribute( VOLUMERESLICEDRIVER_MODE_ATTRIBUTE, modeSS.str().c_str() );

  this->UpdateSliceIfObserved( sliceNode );
}


void vtkSlicerVolumeResliceDriverLogic
::SetRotationForSlice( double rotation, vtkMRMLSliceNode* sliceNode )
{
  if ( sliceNode == NULL )
    {
    return;
    }

  std::stringstream rotationSs;
  rotationSs << rotation;
  sliceNode->SetAttribute( VOLUMERESLICEDRIVER_ROTATION_ATTRIBUTE, rotationSs.str().c_str() );

  this->UpdateSliceIfObserved( sliceNode );
}


void vtkSlicerVolumeResliceDriverLogic
::SetFlipForSlice( bool flip, vtkMRMLSliceNode* sliceNode )
{
  if ( sliceNode == NULL )
    {
    return;
    }

  std::stringstream flipSs;
  flipSs << flip;
  sliceNode->SetAttribute( VOLUMERESLICEDRIVER_FLIP_ATTRIBUTE, flipSs.str().c_str() );

  this->UpdateSliceIfObserved( sliceNode );
}


void vtkSlicerVolumeResliceDriverLogic
::AddObservedNode( vtkMRMLTransformableNode* node )
{
  if (!node)
  {
    return;
  }
  for ( unsigned int i = 0; i < this->ObservedNodes.size(); ++ i )
    {
    if ( node == this->ObservedNodes[ i ] )
      {
      return;
      }
    }

  // Collect event IDs that we need to observe to detect changes in the driving node
  vtkNew<vtkIntArray> modifiedEventsToObserve;

  // Default events
  modifiedEventsToObserve->InsertNextValue(vtkCommand::ModifiedEvent);
  modifiedEventsToObserve->InsertNextValue(vtkMRMLTransformableNode::TransformModifiedEvent);

  // Custom modified events
  vtkIntArray* customContentModifiedEvents = node->GetContentModifiedEvents();
  if (customContentModifiedEvents)
  {
    for (vtkIdType i = 0; i < customContentModifiedEvents->GetNumberOfValues(); i++)
    {
      int eventId = customContentModifiedEvents->GetValue(i);
      if (modifiedEventsToObserve->LookupValue(eventId) >= 0)
      {
        // already included
        continue;
      }
      modifiedEventsToObserve->InsertNextValue(eventId);
    }
  }

  int wasModifying = this->StartModify();
  vtkMRMLTransformableNode* newNode = NULL;
  vtkSetAndObserveMRMLNodeEventsMacro(newNode, node, modifiedEventsToObserve);
  this->ObservedNodes.push_back( newNode );
  this->EndModify( wasModifying );
}


void vtkSlicerVolumeResliceDriverLogic
::ClearObservedNodes()
{
  for ( unsigned int i = 0; i < this->ObservedNodes.size(); ++ i )
    {
    vtkSetAndObserveMRMLNodeMacro( this->ObservedNodes[ i ], 0 );
    }

  this->ObservedNodes.clear();
}



void vtkSlicerVolumeResliceDriverLogic::SetMRMLSceneInternal(vtkMRMLScene * newScene)
{
  vtkNew<vtkIntArray> events;
  events->InsertNextValue(vtkMRMLScene::NodeAddedEvent);
  events->InsertNextValue(vtkMRMLScene::NodeRemovedEvent);
  events->InsertNextValue(vtkMRMLScene::EndBatchProcessEvent);
  this->SetAndObserveMRMLSceneEventsInternal(newScene, events.GetPointer());
}

//-----------------------------------------------------------------------------
void vtkSlicerVolumeResliceDriverLogic::RegisterNodes()
{
}



/// This method is called by Slicer after each significant MRML scene event (import, load, etc.)
void vtkSlicerVolumeResliceDriverLogic
::UpdateFromMRMLScene()
{
  if (this->GetMRMLScene()==NULL)
    {
    vtkErrorMacro("UpdateFromMRMLScene failed: scene is invalid");
    return;
    }

  // Check if any of the slice nodes contain driver transforms that need to be observed.

  vtkCollection* sliceNodes = this->GetMRMLScene()->GetNodesByClass( "vtkMRMLSliceNode" );
  vtkCollectionIterator* sliceIt = vtkCollectionIterator::New();
  sliceIt->SetCollection( sliceNodes );
  for ( sliceIt->InitTraversal(); ! sliceIt->IsDoneWithTraversal(); sliceIt->GoToNextItem() )
    {
    vtkMRMLSliceNode* slice = vtkMRMLSliceNode::SafeDownCast( sliceIt->GetCurrentObject() );
    if ( slice == NULL )
      {
      continue;
      }
    const char* driverCC = slice->GetAttribute( VOLUMERESLICEDRIVER_DRIVER_ATTRIBUTE );
    if ( driverCC == NULL )
      {
      continue;
      }
    vtkMRMLNode* driverNode = this->GetMRMLScene()->GetNodeByID( driverCC );
    if ( driverNode == NULL )
      {
      continue;
      }
    vtkMRMLTransformableNode* driverTransformable = vtkMRMLTransformableNode::SafeDownCast( driverNode );
    if ( driverTransformable == NULL )
      {
      continue;
      }
    this->AddObservedNode( driverTransformable );
    }
  sliceIt->Delete();
  sliceNodes->Delete();
  this->Modified();
}

//---------------------------------------------------------------------------
void vtkSlicerVolumeResliceDriverLogic
::OnMRMLSceneNodeAdded(vtkMRMLNode* vtkNotUsed(node))
{
}

//---------------------------------------------------------------------------
void vtkSlicerVolumeResliceDriverLogic
::OnMRMLSceneNodeRemoved(vtkMRMLNode* vtkNotUsed(node))
{
}



void vtkSlicerVolumeResliceDriverLogic
::OnMRMLNodeModified( vtkMRMLNode* vtkNotUsed(node) )
{
  std::cout << "Observed node modified." << std::endl;
}



void vtkSlicerVolumeResliceDriverLogic
::ProcessMRMLNodesEvents( vtkObject* caller, unsigned long event, void * callData )
{
  if ( caller == NULL )
    {
    return;
    }

  if (    event != vtkMRMLTransformableNode::TransformModifiedEvent
       && event != vtkCommand::ModifiedEvent
       && event != vtkMRMLVolumeNode::ImageDataModifiedEvent )
    {
    this->Superclass::ProcessMRMLNodesEvents( caller, event, callData );
    }

  vtkMRMLTransformableNode* callerNode = vtkMRMLTransformableNode::SafeDownCast( caller );
  if ( callerNode == NULL )
    {
    return;
    }

  std::string callerNodeID( callerNode->GetID() );

  //vtkMRMLNode* node = 0;
  std::vector< vtkMRMLSliceNode* > SlicesToDrive;

  vtkCollection* sliceNodes = this->GetMRMLScene()->GetNodesByClass( "vtkMRMLSliceNode" );
  vtkCollectionIterator* sliceIt = vtkCollectionIterator::New();
  sliceIt->SetCollection( sliceNodes );
  sliceIt->InitTraversal();
  for ( int i = 0; i < sliceNodes->GetNumberOfItems(); ++ i )
    {
    vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast( sliceIt->GetCurrentObject() );
    sliceIt->GoToNextItem();
    const char* driverCC = sliceNode->GetAttribute( VOLUMERESLICEDRIVER_DRIVER_ATTRIBUTE );
    if (    sliceNode != NULL
         && driverCC != NULL
         && callerNodeID.compare( std::string( driverCC ) ) == 0 )
      {
      SlicesToDrive.push_back( sliceNode );
      }
    }
  sliceIt->Delete();
  sliceNodes->Delete();

  for ( unsigned int i = 0; i < SlicesToDrive.size(); ++ i )
    {
    this->UpdateSliceByTransformableNode( callerNode, SlicesToDrive[ i ] );
    }
}



void vtkSlicerVolumeResliceDriverLogic
::UpdateSliceByTransformableNode( vtkMRMLTransformableNode* tnode, vtkMRMLSliceNode* sliceNode )
{
  vtkMRMLLinearTransformNode* transformNode = vtkMRMLLinearTransformNode::SafeDownCast( tnode );
  if ( transformNode != NULL )
    {
    this->UpdateSliceByTransformNode( transformNode, sliceNode );
    }

  vtkMRMLScalarVolumeNode* imageNode = vtkMRMLScalarVolumeNode::SafeDownCast( tnode );
  if ( imageNode != NULL )
    {
    this->UpdateSliceByImageNode( imageNode, sliceNode );
    }

  vtkMRMLMarkupsNode* markupsNode = vtkMRMLMarkupsNode::SafeDownCast(tnode);
  if (markupsNode != NULL)
    {
    this->UpdateSliceByMarkupsNode(markupsNode, sliceNode);
    }

  vtkMRMLAnnotationRulerNode* rulerNode = vtkMRMLAnnotationRulerNode::SafeDownCast( tnode );
  if ( rulerNode != NULL )
    {
    this->UpdateSliceByRulerNode( rulerNode, sliceNode );
    }
}



void vtkSlicerVolumeResliceDriverLogic
::UpdateSliceByTransformNode( vtkMRMLLinearTransformNode* tnode, vtkMRMLSliceNode* sliceNode )
{
  if ( ! tnode)
    {
    return;
    }

  vtkSmartPointer< vtkMatrix4x4 > transform = vtkSmartPointer< vtkMatrix4x4 >::New();
  transform->Identity();
  int getTransf = tnode->GetMatrixTransformToWorld( transform );
  if( getTransf != 0 )
    {
    this->UpdateSlice( transform, sliceNode );
    }
}


void vtkSlicerVolumeResliceDriverLogic
::UpdateSliceByImageNode( vtkMRMLScalarVolumeNode* inode, vtkMRMLSliceNode* sliceNode )
{
  vtkMRMLVolumeNode* volumeNode = inode;

  if (volumeNode == NULL)
    {
    return;
    }

  vtkSmartPointer<vtkMatrix4x4> rtimgTransform = vtkSmartPointer<vtkMatrix4x4>::New();
  volumeNode->GetIJKToRASMatrix(rtimgTransform);

  float tx = rtimgTransform->GetElement(0, 0);
  float ty = rtimgTransform->GetElement(1, 0);
  float tz = rtimgTransform->GetElement(2, 0);
  float sx = rtimgTransform->GetElement(0, 1);
  float sy = rtimgTransform->GetElement(1, 1);
  float sz = rtimgTransform->GetElement(2, 1);
  float nx = rtimgTransform->GetElement(0, 2);
  float ny = rtimgTransform->GetElement(1, 2);
  float nz = rtimgTransform->GetElement(2, 2);
  float px = rtimgTransform->GetElement(0, 3);
  float py = rtimgTransform->GetElement(1, 3);
  float pz = rtimgTransform->GetElement(2, 3);

  int size[3]={0};
  vtkImageData* imageData = volumeNode->GetImageData();
  // imageData may be NULL if volume reslice driver is active while loading an image.
  // Slice position and orientation is stored in the node, so we can still update the slice
  // pose.
  if (imageData!=NULL)
    {
    imageData->GetDimensions(size);
    }

  // normalize
  float psi = sqrt(tx*tx + ty*ty + tz*tz);
  float psj = sqrt(sx*sx + sy*sy + sz*sz);
  float psk = sqrt(nx*nx + ny*ny + nz*nz);
  float ntx = tx / psi;
  float nty = ty / psi;
  float ntz = tz / psi;
  float nsx = sx / psj;
  float nsy = sy / psj;
  float nsz = sz / psj;
  float nnx = nx / psk;
  float nny = ny / psk;
  float nnz = nz / psk;

  // Shift the center
  // NOTE: The center of the image should be shifted due to different
  // definitions of image origin between VTK (Slicer) and OpenIGTLink;
  // OpenIGTLink image has its origin at the center, while VTK image
  // has one at the corner.

  float hfovi = psi * size[0] / 2.0;
  float hfovj = psj * size[1] / 2.0;
  //float hfovk = psk * imgheader->size[2] / 2.0;
  float hfovk = 0;

  float cx = ntx * hfovi + nsx * hfovj + nnx * hfovk;
  float cy = nty * hfovi + nsy * hfovj + nny * hfovk;
  float cz = ntz * hfovi + nsz * hfovj + nnz * hfovk;

  rtimgTransform->SetElement(0, 0, ntx);
  rtimgTransform->SetElement(1, 0, nty);
  rtimgTransform->SetElement(2, 0, ntz);
  rtimgTransform->SetElement(0, 1, nsx);
  rtimgTransform->SetElement(1, 1, nsy);
  rtimgTransform->SetElement(2, 1, nsz);
  rtimgTransform->SetElement(0, 2, nnx);
  rtimgTransform->SetElement(1, 2, nny);
  rtimgTransform->SetElement(2, 2, nnz);
  rtimgTransform->SetElement(0, 3, px + cx);
  rtimgTransform->SetElement(1, 3, py + cy);
  rtimgTransform->SetElement(2, 3, pz + cz);

  vtkMRMLLinearTransformNode* parentNode =
    vtkMRMLLinearTransformNode::SafeDownCast(volumeNode->GetParentTransformNode());
  if (parentNode)
    {
    vtkSmartPointer<vtkMatrix4x4> parentTransform = vtkSmartPointer<vtkMatrix4x4>::New();
    parentTransform->Identity();
    int r = parentNode->GetMatrixTransformToWorld(parentTransform);
    if (r)
      {
      vtkSmartPointer<vtkMatrix4x4> transform = vtkSmartPointer<vtkMatrix4x4>::New();
      vtkMatrix4x4::Multiply4x4(parentTransform, rtimgTransform,  transform);
      this->UpdateSlice( transform, sliceNode );
      return;
      }
    }

  this->UpdateSlice( rtimgTransform, sliceNode );

}

void vtkSlicerVolumeResliceDriverLogic
::UpdateSliceByMarkupsNode(vtkMRMLMarkupsNode* markupsNode, vtkMRMLSliceNode* sliceNode)
{
  vtkMRMLMarkupsPlaneNode* planeNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(markupsNode);
  if (planeNode)
  {
    vtkNew<vtkMatrix4x4> planeToRasMatrix;
    planeNode->GetObjectToWorldMatrix(planeToRasMatrix);
    this->UpdateSlice(planeToRasMatrix, sliceNode);
    return;
  }

  vtkMRMLMarkupsLineNode* lineNode = vtkMRMLMarkupsLineNode::SafeDownCast(markupsNode);
  if (lineNode)
  {
    double position1[3];
    double position2[3];
    lineNode->GetNthControlPointPositionWorld(0, position1);
    lineNode->GetNthControlPointPositionWorld(1, position2);
    this->UpdateSliceByLine(position1, position2, sliceNode);
    return;
  }
  vtkMRMLMarkupsFiducialNode* fidNode = vtkMRMLMarkupsFiducialNode::SafeDownCast(markupsNode);
  if(fidNode)
  {
    double position1[3];
    fidNode->GetNthControlPointPositionWorld(0,position1);
    this->UpdateSliceByPoint(position1, sliceNode);
    return;
  }

}

/*
void Cross(double *a, double *b, double *c)
{
    a[0] = b[1]*c[2] - c[1]*b[2];
    a[1] = c[0]*b[2] - b[0]*c[2];
    a[2] = b[0]*c[1] - c[0]*b[1];
}
*/

void vtkSlicerVolumeResliceDriverLogic
::UpdateSliceByRulerNode( vtkMRMLAnnotationRulerNode* rnode, vtkMRMLSliceNode* sliceNode )
{
  double position1[4];
  double position2[4];
  rnode->GetPositionWorldCoordinates1(position1);
  rnode->GetPositionWorldCoordinates2(position2);
  this->UpdateSliceByLine(position1, position2, sliceNode);
}
void vtkSlicerVolumeResliceDriverLogic
::UpdateSliceByPoint(double posistion1[3],vtkMRMLSliceNode*sliceNode)
{
    vtkNew<vtkMatrix4x4> pointTransform;
    pointTransform->Identity(); // 设置为单位矩阵
    pointTransform->SetElement(0, 3, posistion1[0]);
    pointTransform->SetElement(1, 3, posistion1[1]);
    pointTransform->SetElement(2, 3, posistion1[2]);
    this->UpdateSlice(pointTransform,sliceNode);
}
void vtkSlicerVolumeResliceDriverLogic
::UpdateSliceByLine(double position1[3], double position2[3], vtkMRMLSliceNode* sliceNode)
{
  double t[3];
  double s[3];
  double n[3];
  double nlen;

  // Calculate <n> and normalize it.
  n[0] = position2[0] - position1[0];
  n[1] = position2[1] - position1[1];
  n[2] = position2[2] - position1[2];
  nlen = sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
  if (nlen > 0)
  {
    n[0] /= nlen;
    n[1] /= nlen;
    n[2] /= nlen;
  }
  else
  {
    n[0] = 0.0;
    n[1] = 0.0;
    n[2] = 1.0;
  }

  // Check if <n> is not parallel to <s>=(0.0, 1.0, 0.0)
  if (n[1] < 1.0)
  {
    s[0] = 0.0;
    s[1] = 1.0;
    s[2] = 0.0;
    vtkMath::Cross(s, n, t);
    vtkMath::Cross(n, t, s);
  }
  else
  {
    t[0] = 1.0;
    t[1] = 0.0;
    t[2] = 0.0;
    vtkMath::Cross(n, t, s);
    vtkMath::Cross(s, n, t);
  }

  // normal vectors
  double ntx = t[0];
  double nty = t[1];
  double ntz = t[2];
  double nsx = s[0];
  double nsy = s[1];
  double nsz = s[2];
  double nnx = n[0];
  double nny = n[1];
  double nnz = n[2];
  double px = position2[0];
  double py = position2[1];
  double pz = position2[2];

  vtkNew<vtkMatrix4x4> lineTransform;
  lineTransform->SetElement(0, 0, ntx);
  lineTransform->SetElement(1, 0, nty);
  lineTransform->SetElement(2, 0, ntz);
  lineTransform->SetElement(0, 1, nsx);
  lineTransform->SetElement(1, 1, nsy);
  lineTransform->SetElement(2, 1, nsz);
  lineTransform->SetElement(0, 2, nnx);
  lineTransform->SetElement(1, 2, nny);
  lineTransform->SetElement(2, 2, nnz);
  lineTransform->SetElement(0, 3, px);
  lineTransform->SetElement(1, 3, py);
  lineTransform->SetElement(2, 3, pz);

  this->UpdateSlice(lineTransform, sliceNode);
}



/**
 * Updates the SliceToRAS matrix.
 * SliceToRAS is concatenated from SliceToDriver and DriverToRAS.
 * SliceToDriver depends how we want to orient the slice relative to the
 * driver object (in-plane, transverse, etc.)
 */
void vtkSlicerVolumeResliceDriverLogic
::UpdateSlice(vtkMatrix4x4* driverToRASMatrix, vtkMRMLSliceNode* sliceNode)
{
  // Default values determining the default SliceToDriver transform.
  int mode = MODE_NONE;
  int rotation = 0;
  int flip = 0;


  // Default values for SliceToDriver can be modified by driver node attributes. Read them.

  const char* modeCC = sliceNode->GetAttribute(VOLUMERESLICEDRIVER_MODE_ATTRIBUTE);
  if (modeCC != NULL)
  {
    std::stringstream modeSS(modeCC);
    modeSS >> mode;
  }

  const char* rotationCC = sliceNode->GetAttribute(VOLUMERESLICEDRIVER_ROTATION_ATTRIBUTE);
  if (rotationCC != NULL)
  {
    std::stringstream rotationSS(rotationCC);
    rotationSS >> rotation;
  }

  const char* flipCC = sliceNode->GetAttribute(VOLUMERESLICEDRIVER_FLIP_ATTRIBUTE);
  if (flipCC != NULL)
  {
    std::stringstream flipSS(flipCC);
    flipSS >> flip;
  }

  // SliceToRAS orientation matrix part must be orthonormal
  vtkNew<vtkMatrix4x4> driverToRASMatrixOrthoNormalized;
  double sliceX[3] = { driverToRASMatrix->GetElement(0, 0), driverToRASMatrix->GetElement(1, 0), driverToRASMatrix->GetElement(2, 0) };
  double sliceY[3] = { driverToRASMatrix->GetElement(0, 1), driverToRASMatrix->GetElement(1, 1), driverToRASMatrix->GetElement(2, 1) };
  double sliceZ[3] = { driverToRASMatrix->GetElement(0, 2), driverToRASMatrix->GetElement(1, 2), driverToRASMatrix->GetElement(2, 2) };
  const double tolerance = 0.002; // allows 90+/-0.1 deg angle between axes
  if (fabs(vtkMath::Dot(sliceX, sliceY)) < tolerance
    && fabs(vtkMath::Dot(sliceX, sliceZ)) < tolerance
    && fabs(vtkMath::Dot(sliceY, sliceZ)) < tolerance)
    {
    // Vectors are orthogonal, we just have to make sure they are normalized as well
    for (int i = 0; i < 3; i++)
      {
      // Normalize i-th column vector
      double driverColumn[3] = { driverToRASMatrix->GetElement(0, i), driverToRASMatrix->GetElement(1, i), driverToRASMatrix->GetElement(2, i) };
      vtkMath::Normalize(driverColumn);
      driverToRASMatrixOrthoNormalized->SetElement(0, i, driverColumn[0]);
      driverToRASMatrixOrthoNormalized->SetElement(1, i, driverColumn[1]);
      driverToRASMatrixOrthoNormalized->SetElement(2, i, driverColumn[2]);
      // Copy i-th position component
      driverToRASMatrixOrthoNormalized->SetElement(i, 3, driverToRASMatrix->GetElement(i, 3));
      }
    }
  else
    {
    vtkWarningMacro("Volume reslice driver matrix is not orthonormal. Matrix will be orthonormalized before set in SliceToRAS.");
    double in[3][3];
    double out[3][3];
    for (int i = 0; i < 3; i++)
      {
      in[i][0] = driverToRASMatrix->Element[i][0];
      in[i][1] = driverToRASMatrix->Element[i][1];
      in[i][2] = driverToRASMatrix->Element[i][2];
      }
    // Despite its name, vtkMath::Orthogonalize3x3 performs orthonormalization,
    // not just orthogonalization.
    vtkMath::Orthogonalize3x3(in, out);
    for (int i = 0; i < 3; i++)
      {
      driverToRASMatrixOrthoNormalized->Element[i][0] = out[i][0];
      driverToRASMatrixOrthoNormalized->Element[i][1] = out[i][1];
      driverToRASMatrixOrthoNormalized->Element[i][2] = out[i][2];
      // Copy i-th position component
      driverToRASMatrixOrthoNormalized->SetElement(i, 3, driverToRASMatrix->GetElement(i, 3));
      }
    }

  vtkSmartPointer< vtkTransform > driverToRASTransform = vtkSmartPointer< vtkTransform >::New();
  driverToRASTransform->SetMatrix(driverToRASMatrixOrthoNormalized.GetPointer());
  driverToRASTransform->Update();

  double driverToRasTranslationVector[ 3 ];
  driverToRASTransform->GetPosition( driverToRasTranslationVector );
  vtkSmartPointer< vtkTransform > driverToRasTranslation = vtkSmartPointer< vtkTransform >::New();
  driverToRasTranslation->Identity();
  driverToRasTranslation->Translate( driverToRasTranslationVector );
  driverToRasTranslation->Update();

  double driverToRasOrientationVector[ 4 ];
  driverToRASTransform->GetOrientationWXYZ( driverToRasOrientationVector );
  vtkSmartPointer< vtkTransform > driverToRasRotation = vtkSmartPointer< vtkTransform >::New();
  driverToRasRotation->Identity();
  driverToRasRotation->RotateWXYZ( driverToRasOrientationVector[ 0 ], driverToRasOrientationVector[ 1 ],
    driverToRasOrientationVector[ 2 ], driverToRasOrientationVector[ 3 ] );
  driverToRasRotation->Update();

  vtkSmartPointer< vtkTransform > sliceToDriverTransform = vtkSmartPointer< vtkTransform >::New();
  sliceToDriverTransform->Identity();

  vtkSmartPointer< vtkTransform > sliceToRASTransform = vtkSmartPointer< vtkTransform >::New();
  sliceToRASTransform->Identity();

  switch (mode)
    {
    case MODE_AXIAL:
      sliceToRASTransform->Concatenate( driverToRasTranslation );
      sliceToRASTransform->RotateZ( rotation + 180.0);
      sliceToRASTransform->RotateX( flip * 180.0 + 180.0 );
      sliceToRASTransform->Update();
      break;
    case MODE_SAGITTAL:
      sliceToRASTransform->Concatenate( driverToRasTranslation );
      sliceToRASTransform->RotateX( rotation - 90.0);
      sliceToRASTransform->RotateZ( flip * 180.0 + 180.0 );
      sliceToRASTransform->RotateY( 90 ); // Frist, rotate to sagittal plane.
      sliceToRASTransform->Update();
      break;
    case MODE_CORONAL:
      sliceToRASTransform->Concatenate( driverToRasTranslation );
      sliceToRASTransform->RotateY( rotation + 180.0); // Third, rotate.
      sliceToRASTransform->RotateX( flip * 180.0 + 180.0); // Second, flip.
      sliceToRASTransform->RotateX( 90 ); // First, rotate to coronal plane.
      sliceToRASTransform->Update();
      break;
    case MODE_INPLANE:
      sliceToDriverTransform->RotateX( -90 );
      sliceToDriverTransform->RotateY( 90 );
      sliceToDriverTransform->RotateZ( rotation );
      sliceToDriverTransform->RotateX( flip * 180.0 );
      sliceToRASTransform->Concatenate( driverToRASTransform );
      sliceToRASTransform->Concatenate( sliceToDriverTransform );
      sliceToRASTransform->Update();
      break;
    case MODE_INPLANE90:
      sliceToDriverTransform->RotateX( -90 );
      sliceToDriverTransform->RotateZ( rotation );
      sliceToDriverTransform->RotateX( flip * 180.0 );
      sliceToRASTransform->Concatenate( driverToRASTransform );
      sliceToRASTransform->Concatenate( sliceToDriverTransform );
      sliceToRASTransform->Update();
      break;
    case MODE_TRANSVERSE:
      sliceToDriverTransform->RotateZ( rotation );
      sliceToDriverTransform->RotateX( flip * 180.0 );
      sliceToRASTransform->Concatenate( driverToRASTransform );
      sliceToRASTransform->Concatenate( sliceToDriverTransform );
      sliceToRASTransform->Update();
      break;
    default: //     case MODE_NONE:
      return;
      break;
    };

  sliceNode->GetSliceToRAS()->DeepCopy(sliceToRASTransform->GetMatrix());
  sliceNode->UpdateMatrices();
}


void vtkSlicerVolumeResliceDriverLogic
::UpdateSliceIfObserved( vtkMRMLSliceNode* sliceNode )
{
  if ( sliceNode == NULL )
    {
    return;
    }

  const char* driverCC = sliceNode->GetAttribute( VOLUMERESLICEDRIVER_DRIVER_ATTRIBUTE );
  if ( driverCC == NULL )
    {
    return;
    }

  vtkMRMLNode* node = this->GetMRMLScene()->GetNodeByID( driverCC );

  sliceNode->Modified();
  node->InvokeEvent( vtkMRMLTransformableNode::TransformModifiedEvent );
}

