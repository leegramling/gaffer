//////////////////////////////////////////////////////////////////////////
//  
//  Copyright (c) 2012, John Haddon. All rights reserved.
//  
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//  
//      * Redistributions of source code must retain the above
//        copyright notice, this list of conditions and the following
//        disclaimer.
//  
//      * Redistributions in binary form must reproduce the above
//        copyright notice, this list of conditions and the following
//        disclaimer in the documentation and/or other materials provided with
//        the distribution.
//  
//      * Neither the name of John Haddon nor the names of
//        any other contributors to this software may be used to endorse or
//        promote products derived from this software without specific prior
//        written permission.
//  
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
//  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//  
//////////////////////////////////////////////////////////////////////////

#include "boost/bind.hpp"
#include "boost/bind/placeholders.hpp"

#include "IECore/MatrixTransform.h"

#include "IECoreGL/ToGLCameraConverter.h"
#include "IECoreGL/Camera.h"
#include "IECoreGL/State.h"
#include "IECoreGL/Selector.h"

#include "GafferUI/ViewportGadget.h"
#include "GafferUI/Style.h"

using namespace Imath;
using namespace IECore;
using namespace IECoreGL;
using namespace GafferUI;

IE_CORE_DEFINERUNTIMETYPED( ViewportGadget );

ViewportGadget::ViewportGadget( GadgetPtr child )
	: IndividualContainer( child ),
	  m_cameraController( new IECore::Camera ),
	  m_cameraInMotion( false )
{

	childRemovedSignal().connect( boost::bind( &ViewportGadget::childRemoved, this, ::_1, ::_2 ) );

	buttonPressSignal().connect( boost::bind( &ViewportGadget::buttonPress, this, ::_1,  ::_2 ) );
	buttonReleaseSignal().connect( boost::bind( &ViewportGadget::buttonRelease, this, ::_1,  ::_2 ) );
	buttonDoubleClickSignal().connect( boost::bind( &ViewportGadget::buttonDoubleClick, this, ::_1,  ::_2 ) );
	mouseMoveSignal().connect( boost::bind( &ViewportGadget::mouseMove, this, ::_1,  ::_2 ) );
	dragBeginSignal().connect( boost::bind( &ViewportGadget::dragBegin, this, ::_1, ::_2 ) );
	dragEnterSignal().connect( boost::bind( &ViewportGadget::dragEnter, this, ::_1, ::_2 ) );
	dragMoveSignal().connect( boost::bind( &ViewportGadget::dragMove, this, ::_1, ::_2 ) );
	dropSignal().connect( boost::bind( &ViewportGadget::drop, this, ::_1, ::_2 ) );
	dragEndSignal().connect( boost::bind( &ViewportGadget::dragEnd, this, ::_1, ::_2 ) );
	wheelSignal().connect( boost::bind( &ViewportGadget::wheel, this, ::_1, ::_2 ) );

}

ViewportGadget::~ViewportGadget()
{
}

bool ViewportGadget::acceptsParent( const Gaffer::GraphComponent *potentialParent ) const
{
	return false;
}	

const Imath::V2i &ViewportGadget::getViewport() const
{
	return m_cameraController.getResolution();
}

void ViewportGadget::setViewport( const Imath::V2i &viewport )
{
	m_cameraController.setResolution( viewport );
}

const IECore::Camera *ViewportGadget::getCamera() const
{
	return const_cast<CameraController &>( m_cameraController ).getCamera().get();
}

void ViewportGadget::setCamera( const IECore::Camera *camera )
{
	m_cameraController.setCamera( camera->copy() );
}
		
void ViewportGadget::frame( const Imath::Box3f &box )
{
	m_cameraController.frame( box );
 	renderRequestSignal()( this );
}

void ViewportGadget::frame( const Imath::Box3f &box, const Imath::V3f &viewDirection,
	const Imath::V3f &upVector )
{
 	m_cameraController.frame( box, viewDirection, upVector );
	renderRequestSignal()( this );
}

void ViewportGadget::gadgetsAt( const Imath::V2f &position, std::vector<GadgetPtr> &gadgets )
{
	if( !getChild<Gadget>() )
	{
		return;
	}

	V2f viewport = getViewport();
	V2f regionCentre = position / V2f( viewport.x, viewport.y );
	V2f regionSize = V2f( 2.0 ) / viewport;
	
	Box2f region( regionCentre - regionSize / 2.0f, regionCentre + regionSize / 2.0f );
	
	IECoreGL::ToGLConverterPtr converter = new IECoreGL::ToGLCameraConverter(
 		const_cast<CameraController &>( m_cameraController ).getCamera()
 	);
 	IECoreGL::CameraPtr camera = staticPointerCast<IECoreGL::Camera>( converter->convert() );
 	camera->render( 0 );
	
	glClearColor( 0.3f, 0.3f, 0.3f, 0.0f );
	glClearDepth( 1.0f );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	IECoreGL::Selector selector;
	selector.begin( region );

	const Style *s = style();
	s->bind();
	IndividualContainer::doRender( s );
	
	std::vector<HitRecord> selection;
	selector.end( selection );
	
	/// \todo Figure out how to know when we've been doing depth-tested rendering
	if( false )
	{
		std::sort( selection.begin(), selection.end() );
	}
	else
	{
		std::reverse( selection.begin(), selection.end() );
	}
	
	for( std::vector<HitRecord>::const_iterator it = selection.begin(); it!= selection.end(); it++ )
	{
		GadgetPtr gadget = Gadget::select( it->name.value() );
		if( gadget )
		{
			gadgets.push_back( gadget );
		}
	}
	
	if( !gadgets.size() )
	{
		gadgets.push_back( getChild<Gadget>() );
	}
}

IECore::LineSegment3f ViewportGadget::positionToGadgetSpace( const Imath::V2f &position, const Gadget *gadget ) const
{
	if( !gadget )
	{
		gadget = getChild<Gadget>();
	}

	LineSegment3f result;
	/// \todo The CameraController::unproject() method should be const.
	const_cast<IECore::CameraController &>( m_cameraController ).unproject( V2i( position.x, position.y ), result.p0, result.p1 );
	if( gadget )
	{
		M44f m = gadget->fullTransform();
		m.invert( true );
		result = result * m;
	}
	return result;
}

void ViewportGadget::doRender( const Style *style ) const
{
	glClearColor( 0.3f, 0.3f, 0.3f, 0.0f );
	glClearDepth( 1.0f );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

 	IECoreGL::ToGLConverterPtr converter = new IECoreGL::ToGLCameraConverter(
 		const_cast<CameraController &>( m_cameraController ).getCamera()
 	);
 	IECoreGL::CameraPtr camera = staticPointerCast<IECoreGL::Camera>( converter->convert() );
 	camera->render( 0 );

	IndividualContainer::doRender( style );
}

void ViewportGadget::childRemoved( GraphComponent *parent, GraphComponent *child )
{
	m_lastButtonPressGadget = 0;
	m_gadgetUnderMouse = 0;
}

bool ViewportGadget::buttonPress( GadgetPtr gadget, const ButtonEvent &event )
{
	if( event.modifiers & ModifiableEvent::Alt )
	{
		// accept press so we get a dragBegin opportunity for camera movement
		return true;
	}
	
	std::vector<GadgetPtr> gadgets;
	gadgetsAt( V2f( event.line.p0.x, event.line.p0.y ), gadgets );
			
	GadgetPtr handler = 0;
	bool result = dispatchEvent( gadgets, &Gadget::buttonPressSignal, event, handler );
	if( result )
	{
		m_lastButtonPressGadget = handler;
	}
	else
	{
		m_lastButtonPressGadget = 0;
	}
	
	return result;
}

bool ViewportGadget::buttonRelease( GadgetPtr gadget, const ButtonEvent &event )
{
	bool result = false;
	if( m_lastButtonPressGadget )
	{
		result = dispatchEvent( m_lastButtonPressGadget, &Gadget::buttonReleaseSignal, event );
	}
	
	m_lastButtonPressGadget = 0;	
	return result;
}

bool ViewportGadget::buttonDoubleClick( GadgetPtr gadget, const ButtonEvent &event )
{
	/// \todo Implement me. I'm not sure who this event should go to - probably
	/// the last button press gadget, but we erased that in buttonRelease.
	return false;
}

bool ViewportGadget::mouseMove( GadgetPtr gadget, const ButtonEvent &event )
{
	// find the gadget under the mouse
	std::vector<GadgetPtr> gadgets;
	gadgetsAt( V2f( event.line.p0.x, event.line.p0.y ), gadgets );
	
	GadgetPtr newGadgetUnderMouse = 0;
	if( gadgets.size() )
	{
		newGadgetUnderMouse = gadgets[0];
	}
	
	if( m_gadgetUnderMouse == newGadgetUnderMouse )
	{
		// nothing to be done
		return true;
	}
	
	// figure out the lowest point in the hierarchy where the entered status is unchanged.
	GadgetPtr lowestUnchanged = this;
	if( m_gadgetUnderMouse && newGadgetUnderMouse )
	{
		if( m_gadgetUnderMouse->isAncestorOf( newGadgetUnderMouse ) )
		{
			lowestUnchanged = m_gadgetUnderMouse;		
		}
		else if( newGadgetUnderMouse->isAncestorOf( m_gadgetUnderMouse ) )
		{
			lowestUnchanged = newGadgetUnderMouse;
		}
		else
		{
			lowestUnchanged = m_gadgetUnderMouse->commonAncestor<Gadget>( newGadgetUnderMouse );
		}
	}
		
	// emit leave events, innermost first
	if( m_gadgetUnderMouse )
	{
		GadgetPtr leaveTarget = m_gadgetUnderMouse;
		while( leaveTarget != lowestUnchanged )
		{
			dispatchEvent( leaveTarget, &Gadget::leaveSignal, event );
			leaveTarget = leaveTarget->parent<Gadget>();
		}
	}	
	
	// emit enter events, outermost first
	if( newGadgetUnderMouse )
	{
		std::vector<GadgetPtr> enterTargets;
		GadgetPtr enterTarget = newGadgetUnderMouse;
		while( enterTarget != lowestUnchanged )
		{
			enterTargets.push_back( enterTarget );
			enterTarget = enterTarget->parent<Gadget>();
		}
		for( std::vector<GadgetPtr>::const_reverse_iterator it = enterTargets.rbegin(); it!=enterTargets.rend(); it++ )
		{
			dispatchEvent( *it, &Gadget::enterSignal, event );		
		}
	}
	
	// update status
	m_gadgetUnderMouse = newGadgetUnderMouse;
	return true;
}

IECore::RunTimeTypedPtr ViewportGadget::dragBegin( GadgetPtr gadget, const DragDropEvent &event )
{
	if( event.modifiers & ModifiableEvent::Alt )
	{
		// start camera motion
	
		CameraController::MotionType motionType = CameraController::None;
		switch( event.buttons )
		{
			case ButtonEvent::Left :
				motionType = CameraController::Tumble;
				break;
			case ButtonEvent::Middle :
				motionType = CameraController::Track;
				break;
			case ButtonEvent::Right :
				motionType = CameraController::Dolly;
				break;
			default :
				motionType = CameraController::None;
				break;		
		}
		
		const StringData *projection = getCamera()->parametersData()->member<StringData>( "projection" );
		if( motionType == CameraController::Tumble && ( !projection || projection->readable()=="orthographic" ) )
		{
			motionType = CameraController::Track;
		}
		
		if( motionType )
		{
			m_cameraController.motionStart( motionType, V2i( event.line.p1.x, event.line.p1.y ) );
			m_cameraInMotion = true;
			// the const_cast is necessary because we don't want to give all the other
			// Gadget types non-const access to the event, but we do need the ViewportGadget
			// to assign destination and source gadgets. the alternative would be a different
			// set of non-const signals on the ViewportGadget, or maybe even having ViewportGadget
			// not derived from Gadget at all. this seems the lesser of two evils.
			const_cast<DragDropEvent &>( event ).sourceGadget = this;
			return this;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		// see if a child gadget would like to start a drag
		if( m_lastButtonPressGadget )
		{
			RunTimeTypedPtr data = dispatchEvent( m_lastButtonPressGadget, &Gadget::dragBeginSignal, event );
			if( data )
			{
				const_cast<DragDropEvent &>( event ).sourceGadget = m_lastButtonPressGadget;
			}
			return data;
		}
		else
		{
			return 0;
		}
	}
	
	return 0;
}

bool ViewportGadget::dragEnter( GadgetPtr gadget, const DragDropEvent &event )
{
	if( m_cameraInMotion )
	{
		// it's a drag for moving the camera
		return true;
	}
	else
	{
		std::vector<GadgetPtr> gadgets;
		gadgetsAt( V2f( event.line.p0.x, event.line.p0.y ), gadgets );
	
		GadgetPtr handler = 0;
		bool result = dispatchEvent( gadgets, &Gadget::dragEnterSignal, event, handler );
		if( result )
		{
			const_cast<DragDropEvent &>( event ).destinationGadget = handler;
		}
		return result;
	}
}

bool ViewportGadget::dragMove( GadgetPtr gadget, const DragDropEvent &event )
{
	if( m_cameraInMotion )
	{
		m_cameraController.motionUpdate( V2i( event.line.p1.x, event.line.p1.y ) );
 		renderRequestSignal()( this );
		return true;
	}
	else
	{
		std::vector<GadgetPtr> gadgets;
		gadgetsAt( V2f( event.line.p0.x, event.line.p0.y ), gadgets );
		
		// update drag destination	
		GadgetPtr updatedDestination = updatedDragDestination( gadgets, event );
		if( updatedDestination != event.destinationGadget )
		{
			if( event.destinationGadget )
			{
				dispatchEvent( event.destinationGadget, &Gadget::dragLeaveSignal, event );
			}
			const_cast<DragDropEvent &>( event ).destinationGadget = updatedDestination;
		}
		
		// dispatch drag move to current destination
		if( event.destinationGadget )
		{
			return dispatchEvent( event.destinationGadget, &Gadget::dragMoveSignal, event );
		}
	}
	
	return false;
}

GadgetPtr ViewportGadget::updatedDragDestination( std::vector<GadgetPtr> &gadgets, const DragDropEvent &event )
{
	for( std::vector<GadgetPtr>::const_iterator it = gadgets.begin(), eIt = gadgets.end(); it != eIt; it++ )
	{
		GadgetPtr gadget = *it;
		while( gadget && gadget != this )
		{
			if( gadget == event.destinationGadget )
			{
				// no need to emit enter events when the current destination
				// hasn't changed.
				return gadget;
			}
			
			bool result = dispatchEvent( gadget, &Gadget::dragEnterSignal, event, *it );
			if( result )
			{
				return gadget;
			}
			gadget = gadget->parent<Gadget>();
		}
	}
	
	// if there's nothing under the mouse that wants the drag, keep
	// the existing destination if it's also the source.
	if( event.destinationGadget && event.destinationGadget == event.sourceGadget )
	{
		return event.destinationGadget;
	}
	
	// and if that's not the case then give the drag source another chance
	// to become the destination again.
	if( event.sourceGadget )
	{
		if( dispatchEvent( event.sourceGadget, &Gadget::dragEnterSignal, event ) )
		{
			return event.sourceGadget;
		}
	}
	
	// and if that failed, we have no current destination
	return 0;
}

bool ViewportGadget::drop( GadgetPtr gadget, const DragDropEvent &event )
{
	if( m_cameraInMotion )
	{
		return true;
	}
	else
	{
		if( event.destinationGadget )
		{
			return dispatchEvent( event.destinationGadget, &Gadget::dropSignal, event );
		}
		else
		{
			return false;
		}
	}
}

bool ViewportGadget::dragEnd( GadgetPtr gadget, const DragDropEvent &event )
{
	if( m_cameraInMotion )
	{
		m_cameraController.motionEnd( V2i( event.line.p1.x, event.line.p1.y ) );
		m_cameraInMotion = false;
	 	renderRequestSignal()( this );
		return true;
	}
	else
	{
		if( event.sourceGadget )
		{
			return dispatchEvent( event.sourceGadget, &Gadget::dragEndSignal, event );
		}
	}
	return false;
}

bool ViewportGadget::wheel( GadgetPtr gadget, const ButtonEvent &event )
{
	V2i position( event.line.p0.x, event.line.p0.y );
	
	m_cameraController.motionStart( CameraController::Dolly, position );
	position.x += event.wheelRotation * getViewport().x / 200.0f;
	m_cameraController.motionUpdate( position );
	m_cameraController.motionEnd( position );

 	renderRequestSignal()( this );
	
	return true;
}

void ViewportGadget::eventToGadgetSpace( Event &event, Gadget *gadget )
{
	// no need to do anything
}

void ViewportGadget::eventToGadgetSpace( ButtonEvent &event, Gadget *gadget )
{
	event.line = positionToGadgetSpace( V2f( event.line.p0.x, event.line.p0.y ), gadget );
}

template<typename Event, typename Signal>
typename Signal::result_type ViewportGadget::dispatchEvent( std::vector<GadgetPtr> &gadgets, Signal &(Gadget::*signalGetter)(), const Event &event, GadgetPtr &handler )
{
	for( std::vector<GadgetPtr>::const_iterator it = gadgets.begin(), eIt = gadgets.end(); it != eIt; it++ )
	{
		GadgetPtr gadget = *it;
		while( gadget && gadget != this )
		{
			typename Signal::result_type result = dispatchEvent( gadget, signalGetter, event, *it );
			if( result )
			{
				handler = gadget;
				return result;
			}
			gadget = gadget->parent<Gadget>();
		}
	}
	return typename Signal::result_type();
}
		
template<typename Event, typename Signal>
typename Signal::result_type ViewportGadget::dispatchEvent( GadgetPtr gadget, Signal &(Gadget::*signalGetter)(), const Event &event, GadgetPtr leafGadget )
{
	Event transformedEvent( event );
	eventToGadgetSpace( transformedEvent, gadget.get() );
	/// \todo I'm not really sure I like this whole leaf gadget behaviour. Perhaps
	/// we can ditch it and the classes that need to know what the leaf gadget is
	/// can use the viewport to find the gadget under the event? The GraphGadget node
	/// selection behaviour is a good example of where we're currently relying on the
	/// leaf gadget.
	Signal &s = (gadget->*signalGetter)();
	return s( leafGadget ? leafGadget : gadget, transformedEvent );
}