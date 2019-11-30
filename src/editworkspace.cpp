
#include <nanogui/editworkspace.h>
#include <nanogui/messagedialog.h>
#include <nanovg.h>
#include <nanogui/screen.h>

NAMESPACE_BEGIN(nanogui)

static const std::string attrEditorName = "attributeEditor";

EditorWorkspace::EditorWorkspace(Widget* parent, const std::string& id )
  : Widget(parent),
  _currentMode(EditMode::Select),
  _mouseOverMode(EditMode::Select),
  _gridSize(10,10),
  _drawGrid( true ),
  _useGrid(true),
  _running( true )
{
  setId(id);
}

EditorWorkspace::~EditorWorkspace()
{
}

void EditorWorkspace::performLayout(NVGcontext *ctx)
{
  Widget::performLayout(ctx);
}

Vector2i getUpperLeftCorner(const Vector4i& r)
{
  return Vector2i(r.x(), r.y());
}

Vector4i moveRect(const Vector4i& r, const Vector2i& p)
{
  return { r.x() + p.x(), r.y() + p.y(), r.z() + p.x(), r.w() + p.y() };
}

Vector2i getOffsetToChild(Widget* w, Widget* parent)
{
  if (!parent->isMyChildRecursive(w))
    return Vector2i::Zero();

  Vector2i ret = { 0,0 };
  while (w->parent())
  {
    ret += w->parent()->position();
    w = w->parent();
    if (w == parent)
      break;
  }

  return ret;
}

EditorWorkspace::EditMode EditorWorkspace::getModeFromPos( const Vector2i& p )
{
    try
    {
      Vector2i offset = getOffsetToChild(mSelectedElement, this);
      if (mSelectedElement)
      {
        bool canResize = nonEditableElms.count((intptr_t)mSelectedElement) == 0;

        if (canResize && isPointInsideRect( p, editArea.topleft) )
          return EditMode::ResizeTopLeft;
        else if (canResize && isPointInsideRect( p, editArea.topright) )
          return EditMode::ResizeTopRight;
        else if (canResize && isPointInsideRect( p, editArea.bottomleft) )
          return EditMode::ResizeBottomLeft;
        else if (canResize && isPointInsideRect( p, editArea.bottomright) )
          return EditMode::ResizeBottpmRight;
        else if (canResize && isPointInsideRect( p, editArea.top) )
          return EditMode::ResizeTop;
        else if (canResize && isPointInsideRect( p, editArea.bottom) )
          return EditMode::ResizeBottom;
        else if (canResize && isPointInsideRect( p, editArea.left) )
          return EditMode::ResizeLeft;
        else if (canResize && isPointInsideRect( p, editArea.right) )
          return EditMode::ResizeRight;
        else if ( getEditableElementFromPoint(mSelectedElement, p - offset) == mSelectedElement)
          return EditMode::Move;
        else
          return EditMode::Select;
      }

      return EditMode::Select;
    }
    catch ( ... )
    {
        setSelectedElement(nullptr);
        return EditMode::Select;
    }
}

Widget* EditorWorkspace::getEditableElementFromPoint(Widget* start, const Vector2i &point)
{
    Widget* target = nullptr;

    // we have to search from back to front.

    auto rev_it = start->children().rbegin();
    while (rev_it != start->children().rend())
    {
      target = getEditableElementFromPoint((*rev_it),point - start->position());
      if (target)
      {
        if (/*!target->isSubElement() &&*/ isMyChildRecursive(target) && target != this)
          return target;
        else
          target = nullptr;
      }
      ++rev_it;
    }

    if (isPointInsideRect(point, start->rect()))
      target = start;

    return target;
}

void EditorWorkspace::setHoveredElement(Widget *elm)
{
  mElementUnderMouse = elm;
}

void EditorWorkspace::setSelectedElement(Widget *sel)
{
  Widget* focus = findWidget([](Widget* w) -> bool { return w->focused(); });
  // we only give focus back to children
  if (isMyChildRecursive(focus))
    focus = nullptr;

  bool needUpdateSelectedElm = false;
  if (mSelectedElement != this)
  {
    if (mSelectedElement != sel)// && _editorWindow )
    {
      mSelectedElement = sel;
      needUpdateSelectedElm = true;
    }
  }
  else
  {
    mSelectedElement = nullptr;
    needUpdateSelectedElm = true;
  }

  if (focus)
     focus->requestFocus();
  else
     requestFocus();

  if (needUpdateSelectedElm)
    _sendSelectElementChangedEvent();
}

void EditorWorkspace::setWidgetEditable(intptr_t ptr, bool canEdit)
{
  if (canEdit) nonEditableElms.erase(ptr);
  else nonEditableElms.insert(ptr);
}

Widget* EditorWorkspace::getSelectedElement()
{
  return mSelectedElement;
}

void EditorWorkspace::selectNextSibling()
{
  Widget* p=nullptr;

  if (mSelectedElement && mSelectedElement->parent())
    p = mSelectedElement->parent();
  else
    p = parent();

  auto it = p->children().begin();
  // find selected element
  if (mSelectedElement)
  {
    while (*it != mSelectedElement)
      ++it;
  }

  if (it !=p->children().end())
    ++it;

  if (it != p->children().end())
  {
    setSelectedElement(*it);
    _sendSelectElementChangedEvent();
  }
}

void EditorWorkspace::selectPreviousSibling()
{
  Widget* p=0;

  if (mSelectedElement && mSelectedElement->parent())
    p = mSelectedElement->parent();
  else
    p = parent();

  auto it = p->children().begin();
  // find selected element
  if (mSelectedElement)
  {
    while (*it != mSelectedElement)
      ++it;
  }

  if (it != p->children().end())
    ++it;

  if (it != p->children().end())
  {
    setSelectedElement(*it);
    _sendSelectElementChangedEvent();
  }
}

void EditorWorkspace::_createElementsMap( Widget* start, std::map< std::string, Widget* >& mapa )
{
  auto pIter = start->children().begin();
  for ( ; pIter != children().end(); pIter++ )
  {
    if ( Widget* nrpElm = *pIter )
    {
      if ( nrpElm->id().size() > 0 )
          mapa[ nrpElm->id() ] = *pIter;
    }

    if ( (*pIter)->children().size() > 0 )
      _createElementsMap( *pIter, mapa );
  }
}

void EditorWorkspace::setElementName( Widget* elm, bool setText, std::string& wantName )
{
  std::string typeName = elm->wtypename();

  std::map< std::string, Widget* > elmMap;
  _createElementsMap( this, elmMap );

  std::string checkName = wantName;
  if ( elmMap.find( checkName ) != elmMap.end() || !checkName.size() )
  {
    for ( int index=0; ; index++ )
    {
      checkName = typeName + std::string( "_" ) + std::to_string( index );
      if ( elmMap.find( checkName ) == elmMap.end() )
          break;
    }
  }

  if (elm)
    elm->setId( checkName );

  //if ( setText )
  //    elm->setValue("text", checkName);
  wantName = checkName;
}

void EditorWorkspace::bringElementToFront( Widget* elm )
{
  elm->bringToFront();
}

void EditorWorkspace::_sendHoveredElementChangedEvent()
{
  if (mWidgetHoveredCallback)
    mWidgetHoveredCallback(mElementUnderMouse);
}

bool EditorWorkspace::keyboardEvent(int key, int scancode, int action, int modifiers)
{
  if (isKeyboardActionRelease(action))
  {
    int keycode = key2fourcc(key);
    switch (keycode)
    {
    case FOURCCS("KDEL"):
      if (mSelectedElement)
      {
        mSelectedElement->remove();
        setSelectedElement(nullptr);
        mElementUnderMouse = nullptr;

        _sendSelectElementChangedEvent();
        _sendHoveredElementChangedEvent();

        //if (_changesManager)
        //  _changesManager->Update();
      }
      break;

    case FOURCCS("KEYX"):
      if (isKeyboardModifierCtrl(modifiers) && mSelectedElement)
      {
        // cut
        mSelectedElement->remove();
        setSelectedElement(nullptr);
        mElementUnderMouse = nullptr;

        _sendSelectElementChangedEvent();
        _sendHoveredElementChangedEvent();

        //if (_changesManager)
        //  _changesManager->Update();
      }
      break;

    case FOURCCS("KEYN"):
      if (isKeyboardModifierCtrl(modifiers))
        reset();
      break;

    case FOURCCS("KEYB"):
      if (isKeyboardModifierCtrl(modifiers))
        bringElementToFront(mSelectedElement);
      break;

    case FOURCCS("KEYC"):
      // copy
      if (isKeyboardModifierCtrl(modifiers) && mSelectedElement)
      {
        //copySelectedElementJson();
      }
      break;

    case FOURCCS("KEYR"):
      if (isKeyboardModifierCtrl(modifiers))
      {
        preview();
      }
      break;

    case FOURCCS("KEYP"):
      if (isKeyboardModifierCtrl(modifiers) && mSelectedElement)
        setMode(EditMode::SelectNewParent);
      break;

    case FOURCCS("KEYV"):
      // paste
      if (isKeyboardModifierCtrl(modifiers))
      {
        //pasteJsonToSelectedElement();

        //if (_changesManager)
        //  _changesManager->Update();
      }
      break;

    case FOURCCS("KEYZ"):
      if (isKeyboardModifierCtrl(modifiers))
      {
        //if (modifiers & SYSTEM_COMMAND_SHIFT)
        //  _changesManager->redo();
        //else
        //  _changesManager->undo();
      }
      break;

    default:
      break;
    }

    return true;
  }

  return false;
}

bool EditorWorkspace::scrollEvent(const Vector2i &p, const Vector2f &rel)
{
  if (rel.y() > 0)
    selectPreviousSibling();
  else
    selectNextSibling();

  return true;
}

bool EditorWorkspace::mouseMotionEvent(const Vector2i &pp, const Vector2i &rel, int button, int modifiers)
{
  if (_currentMode == EditMode::Select || _currentMode == EditMode::SelectNewParent)
  {
    // highlight the element that the mouse is over
    const auto saveelm = mElementUnderMouse;
    mElementUnderMouse = getEditableElementFromPoint(this, pp);
    if (mElementUnderMouse == this)
      mElementUnderMouse = nullptr;

    if (_currentMode == EditMode::Select)
    {
      _mouseOverMode = getModeFromPos(pp);
      if (_mouseOverMode > EditMode::Move)
        mElementUnderMouse = mSelectedElement;
    }

    if (saveelm != mElementUnderMouse)
      _sendHoveredElementChangedEvent();
  }
  else if (_currentMode == EditMode::Move)
  {
    // get difference
    Vector2i p = pp - _dragStart;

    // apply to top corner
    p = _startMovePos + p;
    if (_useGrid)
    {
      p -= absolutePosition();

      p.x() = (p.x() / _gridSize.x())*_gridSize.x();
      p.y() = (p.y() / _gridSize.y())*_gridSize.y();

      p += absolutePosition();
    }

    Vector2i delta = p - getUpperLeftCorner(_selectedArea);
    _selectedArea.x() += delta.x();
    _selectedArea.y() += delta.y();
    _selectedArea.z() += delta.x();
    _selectedArea.w() += delta.y();

    if (mElementUnderMouse)
    {
      Vector2i elmpos = mElementUnderMouse->position();
      mElementUnderMouse->setPosition(elmpos + delta);
    }
  }
  else if (_currentMode > EditMode::Move)
  {
    // get difference from start position
    Vector2i offset = getOffsetToChild(mSelectedElement, this);
    Vector2i p = pp + (absolutePosition() - offset);

    if (_useGrid)
    {
      Vector2i ap = absolutePosition();
      p -= ap;

      p.x() = (p.x() / _gridSize.x())*_gridSize.x();
      p.y() = (p.y() / _gridSize.y())*_gridSize.y();

      p += ap;
    }

    switch (_currentMode)
    {
    case EditMode::ResizeTop:    _selectedArea.y() = p.y(); break;
    case EditMode::ResizeBottom: _selectedArea.w() = p.y(); break;
    case EditMode::ResizeLeft:   _selectedArea.x() = p.x(); break;
    case EditMode::ResizeRight:  _selectedArea.z() = p.x(); break;
    case EditMode::ResizeTopLeft:_selectedArea.x() = p.x(); _selectedArea.y() = p.y(); break;
    case EditMode::ResizeTopRight: _selectedArea.y() = p.y(); _selectedArea.z() = p.x(); break;
    case EditMode::ResizeBottomLeft: _selectedArea.x() = p.x(); _selectedArea.w() = p.y(); break;
    case EditMode::ResizeBottpmRight: _selectedArea.z() = p.x(); _selectedArea.w() = p.y(); break;
    default: break;
    }

    if (mElementUnderMouse)
    {
      Vector2i parpos = mElementUnderMouse->parent()->absolutePosition();
      Vector2i newpos = Vector2i(_selectedArea.x(), _selectedArea.y()) - parpos;
      Vector2i newsize = Vector2i(_selectedArea.z() - _selectedArea.x(), _selectedArea.w() - _selectedArea.y());
      mElementUnderMouse->setPosition(newpos);
      mElementUnderMouse->setFixedSize(newsize);
      mElementUnderMouse->setSize(newsize);
    }

    return true;
  }

  return false;
}

bool EditorWorkspace::mouseButtonEvent(const Vector2i &pp, int button, bool down, int modifiers)
{
  if (isMouseButtonLeft(button) && down)
  {
    if (_currentMode == EditMode::SelectNewParent)
      return true;

    Vector2i p = pp;
    Widget* newSelection = findWidget(p);

    if (newSelection != this && isMyChildRecursive(newSelection) && mSelectedElement != newSelection) // redirect event
    {
      newSelection->requestFocus();
      setSelectedElement(newSelection);
      _sendSelectElementChangedEvent();
      return true;
    }

    if (_currentMode == EditMode::Select)
    {
      if (mSelectedElement)
      {
        // start moving or dragging
        _currentMode = getModeFromPos(p);

        if (_currentMode == EditMode::Move)
          _startMovePos = mSelectedElement->absolutePosition();

        _dragStart = p;
        _selectedArea = mSelectedElement->absoluteRect();
      }

      if (_currentMode < EditMode::Move)
      {
        // selecting an element...
        auto saveelm = mElementUnderMouse;
        mElementUnderMouse = getEditableElementFromPoint(this, p);

        if (mElementUnderMouse == this)
          mElementUnderMouse = nullptr;

        setSelectedElement(mElementUnderMouse);

        if (saveelm != mElementUnderMouse)
          _sendHoveredElementChangedEvent();
      }
    }
    return true;
  }
  else if (isMouseButtonRight(button) && down)
  {
    if (_currentMode == EditMode::SelectNewParent || _currentMode >= EditMode::Move)
    {
      // cancel dragging
      _currentMode = EditMode::Select;
    }
    return true;
  }
  else if (isMouseButtonLeft(button) && !down)
  {
    if (_currentMode == EditMode::SelectNewParent)
    {
      if (mSelectedElement)
      {
        auto saveelm = mElementUnderMouse;
        mElementUnderMouse = getEditableElementFromPoint(this, pp);
        if (mElementUnderMouse != mSelectedElement)
        {
          auto saveNewParent = mElementUnderMouse;
          auto saveMovedElm = mSelectedElement;

          mElementUnderMouse->addChild(mSelectedElement);
          saveMovedElm->setPosition(0, 0);

          setSelectedElement( saveMovedElm );
          _sendSelectElementChangedEvent();
        }

        if (saveelm != mElementUnderMouse)
          _sendHoveredElementChangedEvent();

        //if (_changesManager)
        //  _changesManager->Update();
      }
      _currentMode = EditMode::Select;
    }
    else if (_currentMode >= EditMode::Move)
    {
      //Widget *sel = mSelectedElement;
      //setSelectedElement(nullptr);

      // move
      //Vector2i p(0, 0);
      //if (sel && sel->parent())
        //p = sel->parent()->absolutePosition();

      //setSelectedElement(sel);
      _currentMode = EditMode::Select;

      //if (_changesManager)
      //  _changesManager->Update();
    }
    return true;
  }

  return false;
}

//! called if an event happened.
/*bool EditorWorkspace::onEvent( const NEvent &e )
{
  switch(e.EventType)
  {
    /*case NRP_DRAGDROP_EVENT:
        if ( e.DragDropEvent.EventType == NRP_DROP_ELEMENT && e.DragDropEvent.Element == _factoryView )
        {
            Point mousePos = e.MouseEvent.getPosition();
            Widget* parentElm = getEditableElementFromPoint(this, mousePos );

            s32 fIndex = _factoryView->GetFactoryIndex();
            s32 elmIndex = _factoryView->GetElementIndex();
            if (  fIndex >= 0 && elmIndex >= 0 )
            {
                AbstractWidgetsFactory* factory = WidgetsFactoriesManager::instance().getFactory( fIndex );
                String name = factory->getWidgetTypeName( elmIndex );

                // add it
                Widget *newElement = factory->createWidget( name, parentElm );
                if ( newElement )
                {
                    Point p = mousePos - parentElm->getAbsoluteRect().UpperLeftCorner;
          newElement->setGeometry( RectI( p, core::NSizeU(100,100) ) );

                    String checkName;
                    setElementName( newElement, true, checkName );
                    setSelectedElement(0);
                    setSelectedElement( newElement );

                    if ( _changesManager )
                        _changesManager->Update();
                }
            }

            getEnvironment()->setDragObject( NULL, Texture(), core::RectI( 0, 0, 0, 0 ) );
            _editorWindow->updateTree( this );
            return true;
        }
    break;
  // even if we didn't absorb the event,
  // we never pass events back to the GUI we're editing!
  return false;
}*/

void EditorWorkspace::preview()
{
    Vector2i scrSize = screen()->size();
    Widget* elm = children().size() > 0 ? ( children().front() ) : nullptr;
    if (!elm)
    {
      new MessageDialog(this, MessageDialog::Type::Information, "Warning", "Have no element to preview");
    }
    else
    {
     /* _factoryView->EnabledEditorMode( false );
          _previewWindow = new Window( this, RectI( Point( 0, 0 ), scrSize ), L"Preview mode", -1 );

      io::FilePath saveFileName( L"temp/forPreview.ui" );
          getEnvironment()->saveUI( saveFileName, elm );

          getEnvironment()->loadUI( saveFileName, _previewWindow );
          _previewWindow->setModal();
          _previewWindow->setNotClipped( true );
      _previewWindow->setWindowFlag( Window::draggable, false );
      _factoryView->EnabledEditorMode( true ); */
    }
}

//! draws the element and its children
void EditorWorkspace::draw(NVGcontext* ctx)
{
  nvgSave(ctx);

  nvgBeginPath(ctx);
  nvgRect(ctx, mPos.x(), mPos.y(), width(), height());
  nvgStrokeColor(ctx, nvgRGBA(0, 0, 0, 255));
  nvgStroke(ctx);

  if ( _drawGrid )
  {
    // draw the grid
    int cy = _gridSize.x();
    nvgBeginPath(ctx);

    int row = 0;
    while (cy < height())
    {
      row++;
      if (row % 5 == 0)
      {
        nvgStroke(ctx);
        nvgBeginPath(ctx);
        nvgStrokeColor(ctx, nvgRGBA(0xE0, 0xE0, 0xE0, 0x20));
      }

      nvgMoveTo(ctx, mPos.x(), mPos.y() + cy);
      nvgLineTo(ctx, mPos.x() + width(), mPos.y() + cy);

      if (row % 5 == 0)
      {
        nvgStroke(ctx);
        nvgBeginPath(ctx);
      }

      nvgStrokeColor(ctx, nvgRGBA(0xB0, 0xB0, 0xB0, 0x20));
      cy += _gridSize.y();
    }
    nvgStroke(ctx);

    int cx = _gridSize.x();
    int col = 0;

    nvgBeginPath(ctx);
    while (cx < width())
    {
      col++;
      if (col % 5 == 0)
      {
        nvgStroke(ctx);
        nvgBeginPath(ctx);
        nvgStrokeColor(ctx, nvgRGBA(0xE0, 0xE0, 0xE0, 0x20));
      }

      nvgMoveTo(ctx, mPos.x() + cx, mPos.y());
      nvgLineTo(ctx, mPos.x() + cx, mPos.y() + height());

      if (col % 5 == 0)
      {
        nvgStroke(ctx);
        nvgBeginPath(ctx);
      }

      nvgStrokeColor(ctx, nvgRGBA(0xB0, 0xB0, 0xB0, 0x20));
      cx += _gridSize.x();
    }
    nvgStroke(ctx);
  }

  nvgRestore(ctx);

  Widget::draw(ctx);

  _drawSelectedElement(ctx);
  _drawResizePoints(ctx);
}

void EditorWorkspace::addChild(int index, Widget * widget)
{
  Widget::addChild(index, widget);

  if (mChildrenChangeCallback)
    mChildrenChangeCallback();
}

void EditorWorkspace::_drawWidthRectangle(NVGcontext* ctx, Color& color, int ww, const Vector4i& r)
{
  nvgBeginPath(ctx);
  nvgStrokeColor(ctx, color);
  nvgRect(ctx, r.x(), r.y(), r.z() - r.x(), r.w() - r.y());
  nvgStroke(ctx);
}

void EditorWorkspace::_drawResizePoint(NVGcontext* ctx, const Color& color, const Vector4i& tr)
{
  nvgBeginPath(ctx);
  nvgFillColor(ctx, color);
  Vector2i c = rect_center(tr);
  float r = (tr.z() - tr.x()) / 2;
  nvgCircle(ctx, c.x(), c.y(), r );
  //nvgRect(ctx, tr.x(), tr.y(), tr.z() - tr.x(), tr.w() - tr.y());
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgStrokeColor(ctx, Color(0, 0, 0, 255));
  nvgCircle(ctx, c.x(), c.y(), r );
  //nvgRect(ctx, tr.x(), tr.y(), tr.z() - tr.x(), tr.w() - tr.y());
  nvgStroke(ctx);
}

void EditorWorkspace::_drawResizePoints(NVGcontext* ctx)
{
    if ( mSelectedElement )
    {
        // draw handles for moving
        EditMode m = _currentMode;
        Vector2i offset = getOffsetToChild(mSelectedElement, this);
        Vector4i r = moveRect(mSelectedElement->rect(), offset);
        if (m < EditMode::Move)
            m = _mouseOverMode;

        int dfHalf = 3;
        Vector2i c = rect_center(r);

        editArea.topleft =  Vector4i(r.x() - dfHalf, r.y() - dfHalf, r.x() + dfHalf, r.y() + dfHalf);
        editArea.topright = Vector4i(r.z() - dfHalf, r.y() - dfHalf, r.z() + dfHalf, r.y() + dfHalf);
        editArea.top = Vector4i(c.x() - dfHalf, r.y() - dfHalf, c.x() + dfHalf, r.y() + dfHalf);
        editArea.bottomleft = Vector4i(r.x() - dfHalf, r.w() - dfHalf, r.x() + dfHalf, r.w() + dfHalf);
        editArea.left = Vector4i(r.x() - dfHalf, c.y() - dfHalf, r.x() + dfHalf, c.y() + dfHalf);
        editArea.right = Vector4i(r.z() - dfHalf, c.y() - dfHalf, r.z() + dfHalf, c.y() + dfHalf);
        editArea.bottomright = Vector4i(r.z() - dfHalf, r.w() - dfHalf, r.z() + dfHalf, r.w() + dfHalf);
        editArea.bottom = Vector4i(c.x() - dfHalf, r.w() - dfHalf, c.x() + dfHalf, r.w() + dfHalf);

        // top left
        Color bg(180,0, 0, 255);
        Color select(180, 255, 0, 255);
        bool avoidedElm = nonEditableElms.count((intptr_t)mSelectedElement);
        if (_currentMode == EditMode::Move || avoidedElm)
          bg = select = Color(64, 64);

        bool mySide = (m == EditMode::ResizeTop || m == EditMode::ResizeLeft || m == EditMode::ResizeTopLeft);
        _drawResizePoint(ctx, mySide ? select : bg, editArea.topleft );

        mySide = (m == EditMode::ResizeTop || m == EditMode::ResizeRight || m == EditMode::ResizeTopRight);
        _drawResizePoint(ctx, mySide ? select : bg, editArea.topright );

        mySide = (m == EditMode::ResizeTop);
        _drawResizePoint( ctx, mySide ? select : bg, editArea.top);

        mySide = (m == EditMode::ResizeLeft || m == EditMode::ResizeBottomLeft || m == EditMode::ResizeBottom);
        _drawResizePoint( ctx, mySide ? select : bg, editArea.bottomleft);

        mySide = (m == EditMode::ResizeLeft);
        _drawResizePoint( ctx, mySide ? select : bg, editArea.left);

        mySide = (m == EditMode::ResizeRight);
        _drawResizePoint( ctx, mySide ? select : bg, editArea.right);

        mySide = (m ==  EditMode::ResizeRight || m == EditMode::ResizeBottpmRight || m == EditMode::ResizeBottom);
        _drawResizePoint( ctx, mySide ? select : bg, editArea.bottomright);

        mySide = (m == EditMode::ResizeBottom);
        _drawResizePoint( ctx, mySide ? select : bg, editArea.bottom);
    }
}

void EditorWorkspace::_drawSelectedElement(NVGcontext* ctx)
{
  Vector2i offset = getOffsetToChild(mSelectedElement, this);
  if (mElementUnderMouse &&
      mElementUnderMouse != mSelectedElement &&
      mElementUnderMouse != parent() )
  {
    bool underMouseNotEditable = nonEditableElms.count((intptr_t)mElementUnderMouse);
    auto color = underMouseNotEditable ? Color(0, 0, 64, 255) : Color(100,0,0,255);

    Vector2i umoffset = getOffsetToChild(mElementUnderMouse, this);
    _drawWidthRectangle(ctx, color, 2, moveRect(mElementUnderMouse->rect(), umoffset));
  }

  if (!mSelectedElement)
    return;

  bool avoidedElm = nonEditableElms.count((intptr_t)mSelectedElement);
  if (_currentMode == EditMode::Select)
  {
    auto color = avoidedElm ? Color(0, 0, 64, 255) : Color(100, 0, 0, 255);
    _drawWidthRectangle(ctx, color, 2, moveRect(mSelectedElement->rect(), offset));
  }
  else if (_currentMode == EditMode::SelectNewParent)
  {
    float value = std::fmod((float)getTimeFromStart(), 1.0f);
    Vector4i r = moveRect(mSelectedElement->rect(), offset);
    Color color(0, 0, 255, value > 0.5 ? 32 : 64);
    nvgBeginPath(ctx);
    nvgFillColor(ctx, color);
    nvgRect(ctx, r.x(), r.y(), r.z() - r.x(), r.w() - r.y());
    nvgFill(ctx);
  }
  else if (_currentMode >= EditMode::Move)
  {
    Vector4i tr = moveRect(mSelectedElement->rect(), offset);;
    Color color(255, 0, 0, 64);
    nvgBeginPath(ctx);
    nvgFillColor(ctx, color);
    nvgRect(ctx, tr.x(), tr.y(), tr.z() - tr.x(), tr.w() - tr.y());
    nvgFill(ctx);
  }
}

void EditorWorkspace::setGridVisible( bool visible )
{
  _drawGrid = visible;
}

void EditorWorkspace::setGridSize(const Vector2i& gridSize)
{
  _gridSize = gridSize;
  if (_gridSize.x() < 2)
    _gridSize.x() = 2;
  if (_gridSize.y()< 2)
    _gridSize.y() = 2;
}

void EditorWorkspace::setUseGrid(bool useGrid)
{
  _useGrid = useGrid;
}

void EditorWorkspace::copySelectedElementToJson()
{
  // create memory write file
  //io::ArrayMappedFile memWrite( L"#Clipboard#" );
  // save gui to mem file
  //getEnvironment()->saveUI( &memWrite, _selectedElement );

  // copy to clipboard- wide chars not supported yet :(
  //String text( memWrite.getData() );
  //getEnvironment()->getOSOperator()->copyToClipboard( text );
}

void EditorWorkspace::pasteJsonToSelectedElement()
{
  // get clipboard data
  //String XMLText = getEnvironment()->getOSOperator()->getTextFromClipboard();

  //io::ArrayMappedFile memFile("#Clipboard#");

  // write clipboard data
  //memFile.write( XMLText.toAscii().pointer(), XMLText.size() );

  // rewind file
  //memFile.seek(0, false);

  // read xml
  //getEnvironment()->loadUI( &memFile, _selectedElement);

  // reset focus
  //getEnvironment()->setFocus(this);
}

void EditorWorkspace::save(Serializer& out) const
{
  //out->AddBool( L"DrawGrid", drawGrid_);
  //out->AddBool( L"UseGrid", UseGrid);
  //out->AddPosition2d( L"GridSize", Point(GridSize.Width, GridSize.Height));
  //out->AddInt( L"MenuCommandStart", MenuCommandStart);
}

bool EditorWorkspace::load(Serializer& in )
{
  //setGridVisible(in->getAttributeAsBool(L"DrawGrid"));
  //setUseGrid(in->getAttributeAsBool(L"UseGrid"));

  //Point tmpp = in->getAttributeAsPosition2d(L"GridSize");
  //core::NSizeU tmpd(tmpp.X, tmpp.Y);
  //setGridSize(tmpd);
  //setMenuCommandIDStart(in->getAttributeAsInt( L"MenuCommandStart"));
  return true;
}

std::string EditorWorkspace::wtypename() const
{
    return "EditorWorkspace";
}

void EditorWorkspace::setFactoryView( FactoryView* wnd )
{
  _factoryView = wnd;
}

void EditorWorkspace::_sendSelectElementChangedEvent()
{
  for (auto& func : mSelectedCallbacks)
    if (func) func(mSelectedElement);
}

void EditorWorkspace::reset()
{
  setSelectedElement(nullptr);
  mElementUnderMouse = nullptr;

  _sendSelectElementChangedEvent();
  _sendHoveredElementChangedEvent();

  while ( !children().empty() )
    removeChild( children().front() );
}

void EditorWorkspace::cutSelectedElement()
{
  copySelectedElementToJson();
  removeElement(mSelectedElement);
}

void EditorWorkspace::removeElement( Widget* elm )
{
  Widget* saveElm = elm;

  if (elm == mSelectedElement)
  {
    setSelectedElement(nullptr);
    _sendSelectElementChangedEvent();
  }

  if (elm == mElementUnderMouse)
  {
    mElementUnderMouse = nullptr;
    _sendHoveredElementChangedEvent();
  }

  saveElm->remove();
}

void EditorWorkspace::setMode( EditMode mode )
{
  _currentMode = mode;
}

void EditorWorkspace::updateTree()
{
    //_editorWindow->updateTree( this );
}

void EditorWorkspace::undo()
{
    //if ( _changesManager )
      //  _changesManager->Undo();
}

void EditorWorkspace::redo()
{
    //if ( _changesManager )
      //  _changesManager->Redo();
}

bool EditorWorkspace::isUndoEnabled() const
{
    //return _changesManager ? _changesManager->IsUndoEnabled() : false;
  return false;
}

bool EditorWorkspace::isRedoEnabled() const
{
    //return _changesManager ? _changesManager->IsRedoEnabled() : false;
  return false;
}

void EditorWorkspace::update()
{
    //if ( _changesManager )
      //  _changesManager->Update();
}

void EditorWorkspace::removeChild(const Widget* child )
{
  if (child == mSelectedElement)
  {
    setSelectedElement(nullptr);
    _sendSelectElementChangedEvent();
  }

  if (child == mElementUnderMouse)
  {
    mElementUnderMouse = nullptr;
    _sendHoveredElementChangedEvent();
  }

  Widget::removeChild( child );
}

void EditorWorkspace::activateChangeParentMode()
{
    setMode( EditMode::SelectNewParent );
}

void EditorWorkspace::toggleOptionsVisible()
{
    /*if ( _optionsWindow )
    {
        _optionsWindow->deleteLater();
        _optionsWindow = NULL;
        return;
    }

    _optionsWindow = new Window( NULL, RectI(0, 0, 0, 0), L"Workspace options", -1 );
    _optionsWindow->setRelativeRectProportional( RectF(0.3f, 0.25f, 0.6f, 0.75f) );
    _optionsWindow->getButton( Window::buttonClose )->setVisible( false );

    AttributeEditor* editor = (AttributeEditor*)WidgetsFactoriesManager::instance().createWidget( attrEditorName, _optionsWindow);
    editor->setID( attrEditorName.getHash() );
    editor->setRelativeRectProportional( RectF(0.0f, 0.1f, 1.0f, 1.0f));
    editor->setAlignment(alignUpperLeft, alignLowerRight, alignUpperLeft, alignLowerRight);

    save( editor->getAttribs() );
    editor->refreshAttribs();

    CONNECT( editor, onAttributeChanged(), this, EditWorkspace::userChangeOptions );
    */
}

void EditorWorkspace::saveSelectedElementToJson()
{
    Widget* elm = getSelectedElement();

    //getEnvironment()->saveUI( io::FilePath( "temp/testXml.ui" ), elm );
}

void EditorWorkspace::userChangeOptions()
{
    /*if ( _optionsWindow )
    {
        if ( AttributeEditor* editor = safety_cast<AttributeEditor*>( _optionsWindow->findChild( attrEditorName.getHash() ) ) )
            load( editor->getAttribs() );
    }*/
}

bool EditorWorkspace::isGridVisible() const
{
  return _drawGrid;
}

NAMESPACE_END(nanogui)
