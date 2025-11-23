class PaintManager {
  constructor(canvas, ctx) {
    this.canvas = canvas;
    this.ctx = ctx;
    this.painting = false;
    this.lastX = 0;
    this.lastY = 0;
    this.brushColor = "#000000";
    this.brushSize = 2;
    this.currentTool = null;
    this.textElements = [];
    this.lineSegments = [];
    this.isTextPlacementMode = false;
    this.draggingCanvasContext = null;
    this.selectedTextElement = null;
    this.isDraggingText = false;
    this.dragOffsetX = 0;
    this.dragOffsetY = 0;
    this.textBold = false;
    this.textItalic = false;
    this.todoItems = [];
    this.isTodoPlacementMode = false;
    this.selectedTodoItem = null;
    this.todoBold = false;
    this.todoItalic = false;
    this.todoColor = '#000000';
    this.showTodoDeleteButtons = true; // Toggle for showing/hiding delete buttons

    // Schedule (timetable) properties
    this.scheduleData = null; // 2D array to store schedule data
    this.scheduleDays = 5;
    this.scheduleClasses = 6;
    this.scheduleFontFamily = 'SimHei';
    this.scheduleFontSize = 12;
    this.scheduleColor = '#000000';
    this.scheduleStartX = 20;
    this.scheduleStartY = 20;
    this.scheduleCellWidth = 60;
    this.scheduleCellHeight = 35;
    this.weekDays = ['周一', '周二', '周三', '周四', '周五', '周六', '周日'];
    this.selectedScheduleCell = null; // For editing schedule cells
    this.showScheduleCellIndicator = true; // Toggle for showing/hiding cell selection indicator

    // Brush cursor indicator
    this.brushCursor = null;

    // Undo/Redo functionality
    this.historyStack = [];
    this.historyStep = -1;
    this.MAX_HISTORY = 50;

    // Bind event handlers
    this.startPaint = this.startPaint.bind(this);
    this.paint = this.paint.bind(this);
    this.endPaint = this.endPaint.bind(this);
    this.handleCanvasClick = this.handleCanvasClick.bind(this);
    this.onTouchStart = this.onTouchStart.bind(this);
    this.onTouchMove = this.onTouchMove.bind(this);
    this.onTouchEnd = this.onTouchEnd.bind(this);
    this.handleKeyboard = this.handleKeyboard.bind(this);
    this.updateBrushCursor = this.updateBrushCursor.bind(this);
    this.hideBrushCursor = this.hideBrushCursor.bind(this);
  }

  saveToHistory() {
    // Remove any states after current step (when user drew something after undoing)
    this.historyStack = this.historyStack.slice(0, this.historyStep + 1);

    // Save current canvas state along with text and line data
    const canvasState = {
      imageData: this.ctx.getImageData(0, 0, this.canvas.width, this.canvas.height),
      textElements: JSON.parse(JSON.stringify(this.textElements)),
      lineSegments: JSON.parse(JSON.stringify(this.lineSegments)),
      todoItems: JSON.parse(JSON.stringify(this.todoItems)),
      scheduleData: this.scheduleData ? JSON.parse(JSON.stringify(this.scheduleData)) : null
    };

    this.historyStack.push(canvasState);
    this.historyStep++;

    // Limit history size
    if (this.historyStack.length > this.MAX_HISTORY) {
      this.historyStack.shift();
      this.historyStep--;
    }

    this.updateUndoRedoButtons();

    // Auto-save to localStorage
    this.saveCanvasToLocalStorage();
  }

  saveCanvasToLocalStorage() {
    try {
      const canvasData = {
        imageData: this.ctx.getImageData(0, 0, this.canvas.width, this.canvas.height).data.join(','),
        textElements: this.textElements,
        lineSegments: this.lineSegments,
        todoItems: this.todoItems,
        scheduleData: this.scheduleData,
        width: this.canvas.width,
        height: this.canvas.height
      };
      localStorage.setItem('canvasState', JSON.stringify(canvasData));
    } catch (e) {
      console.error('Failed to save canvas to localStorage:', e);
    }
  }

  loadCanvasFromLocalStorage() {
    try {
      const savedData = localStorage.getItem('canvasState');
      if (!savedData) return false;

      const canvasData = JSON.parse(savedData);

      // Verify dimensions match
      if (canvasData.width !== this.canvas.width || canvasData.height !== this.canvas.height) {
        return false;
      }

      // Restore image data
      const imageArray = canvasData.imageData.split(',').map(Number);
      const imageData = this.ctx.createImageData(this.canvas.width, this.canvas.height);
      imageData.data.set(imageArray);
      this.ctx.putImageData(imageData, 0, 0);

      // Restore elements
      this.textElements = canvasData.textElements || [];
      this.lineSegments = canvasData.lineSegments || [];
      this.todoItems = canvasData.todoItems || [];
      this.scheduleData = canvasData.scheduleData || null;

      this.saveToHistory();
      return true;
    } catch (e) {
      console.error('Failed to load canvas from localStorage:', e);
      return false;
    }
  }

  clearCanvasCache() {
    try {
      localStorage.removeItem('canvasState');
    } catch (e) {
      console.error('Failed to clear canvas cache:', e);
    }
  }

  undo() {
    if (this.historyStep > 0) {
      this.historyStep--;
      this.restoreFromHistory();
    }
  }

  redo() {
    if (this.historyStep < this.historyStack.length - 1) {
      this.historyStep++;
      this.restoreFromHistory();
    }
  }

  restoreFromHistory() {
    if (this.historyStep >= 0 && this.historyStep < this.historyStack.length) {
      const state = this.historyStack[this.historyStep];

      // Restore canvas image
      this.ctx.putImageData(state.imageData, 0, 0);

      // Restore text and line data
      this.textElements = JSON.parse(JSON.stringify(state.textElements));
      this.lineSegments = JSON.parse(JSON.stringify(state.lineSegments));
      this.todoItems = JSON.parse(JSON.stringify(state.todoItems || []));
      this.scheduleData = state.scheduleData ? JSON.parse(JSON.stringify(state.scheduleData)) : null;

      this.updateUndoRedoButtons();
    }
  }

  updateUndoRedoButtons() {
    const undoBtn = document.getElementById('undo-btn');
    const redoBtn = document.getElementById('redo-btn');

    if (undoBtn) {
      undoBtn.disabled = this.historyStep <= 0;
    }

    if (redoBtn) {
      redoBtn.disabled = this.historyStep >= this.historyStack.length - 1;
    }
  }

  initPaintTools() {
    document.getElementById('brush-mode').addEventListener('click', () => {
      if (this.currentTool === 'brush') {
        this.setActiveTool(null, '');
      } else {
        this.setActiveTool('brush', '画笔模式');
        this.brushColor = document.getElementById('brush-color').value;
      }
    });

    document.getElementById('eraser-mode').addEventListener('click', () => {
      if (this.currentTool === 'eraser') {
        this.setActiveTool(null, '');
      } else {
        this.setActiveTool('eraser', '橡皮擦');
        this.brushColor = "#FFFFFF";
      }
    });

    document.getElementById('text-mode').addEventListener('click', () => {
      if (this.currentTool === 'text') {
        this.setActiveTool(null, '');
      } else {
        this.setActiveTool('text', '插入文字');
        this.brushColor = document.getElementById('brush-color').value;
      }
    });

    document.getElementById('brush-color').addEventListener('change', (e) => {
      this.brushColor = e.target.value;
    });

    document.getElementById('brush-size').addEventListener('input', (e) => {
      this.brushSize = parseInt(e.target.value);
      this.updateBrushCursorSize();
    });

    document.getElementById('add-text-btn').addEventListener('click', () => this.startTextPlacement());

    document.getElementById('todo-mode').addEventListener('click', () => {
      if (this.currentTool === 'todo') {
        this.setActiveTool(null, '');
      } else {
        // Load cached canvas data if available
        this.loadCanvasFromLocalStorage();
        this.setActiveTool('todo', '添加待办项');
        this.brushColor = document.getElementById('brush-color').value;
      }
    });

    document.getElementById('add-todo-btn').addEventListener('click', () => this.startTodoPlacement());

    document.getElementById('schedule-mode').addEventListener('click', () => {
      if (this.currentTool === 'schedule') {
        this.setActiveTool(null, '');
      } else {
        // Load cached schedule data if available
        this.loadScheduleFromLocalStorage();
        this.setActiveTool('schedule', '生成课表');
      }
    });

    document.getElementById('create-schedule-btn').addEventListener('click', () => this.createSchedule());

    document.getElementById('toggle-schedule-cell-indicator-btn').addEventListener('click', () => {
      this.showScheduleCellIndicator = !this.showScheduleCellIndicator;
      document.getElementById('toggle-schedule-cell-indicator-btn').classList.toggle('primary', this.showScheduleCellIndicator);
      if (this.scheduleData) {
        this.redrawAll();
      }
    });

    document.getElementById('schedule-input-confirm-btn').addEventListener('click', () => this.confirmScheduleInput());
    document.getElementById('schedule-input-cancel-btn').addEventListener('click', () => this.cancelScheduleInput());
    document.getElementById('schedule-input').addEventListener('keypress', (e) => {
      if (e.key === 'Enter') this.confirmScheduleInput();
    });

    // Schedule font size adjustment buttons
    document.getElementById('schedule-font-increase-btn').addEventListener('click', () => {
      this.scheduleFontSize = Math.min(this.scheduleFontSize + 1, 32);
      document.getElementById('schedule-font-size').value = this.scheduleFontSize;
      if (this.scheduleData) {
        this.calculateScheduleDimensions();
        this.redrawAll();
      }
    });

    document.getElementById('schedule-font-decrease-btn').addEventListener('click', () => {
      this.scheduleFontSize = Math.max(this.scheduleFontSize - 1, 8);
      document.getElementById('schedule-font-size').value = this.scheduleFontSize;
      if (this.scheduleData) {
        this.calculateScheduleDimensions();
        this.redrawAll();
      }
    });

    // Schedule font size input change
    document.getElementById('schedule-font-size').addEventListener('change', (e) => {
      this.scheduleFontSize = parseInt(e.target.value);
      if (this.scheduleData) {
        this.calculateScheduleDimensions();
        this.redrawAll();
      }
    });

    // Schedule move buttons
    document.getElementById('schedule-move-up-btn').addEventListener('click', () => {
      this.scheduleStartY = Math.max(this.scheduleStartY - 10, 5);
      if (this.scheduleData) this.redrawAll();
    });

    document.getElementById('schedule-move-down-btn').addEventListener('click', () => {
      const maxY = this.canvas.height - (this.scheduleClasses + 1) * this.scheduleCellHeight - 5;
      this.scheduleStartY = Math.min(this.scheduleStartY + 10, maxY);
      if (this.scheduleData) this.redrawAll();
    });

    document.getElementById('schedule-move-left-btn').addEventListener('click', () => {
      this.scheduleStartX = Math.max(this.scheduleStartX - 10, 5);
      if (this.scheduleData) this.redrawAll();
    });

    document.getElementById('schedule-move-right-btn').addEventListener('click', () => {
      const maxX = this.canvas.width - (this.scheduleDays + 1) * this.scheduleCellWidth - 5;
      this.scheduleStartX = Math.min(this.scheduleStartX + 10, maxX);
      if (this.scheduleData) this.redrawAll();
    });

    // Schedule zoom buttons
    document.getElementById('schedule-zoom-in-btn').addEventListener('click', () => {
      this.scheduleCellWidth = Math.min(this.scheduleCellWidth + 5, 200);
      this.scheduleCellHeight = Math.min(this.scheduleCellHeight + 5, 100);
      if (this.scheduleData) this.redrawAll();
    });

    document.getElementById('schedule-zoom-out-btn').addEventListener('click', () => {
      this.scheduleCellWidth = Math.max(this.scheduleCellWidth - 5, 30);
      this.scheduleCellHeight = Math.max(this.scheduleCellHeight - 5, 20);
      if (this.scheduleData) this.redrawAll();
    });

    // Add event listeners for bold and italic buttons
    document.getElementById('text-bold').addEventListener('click', () => {
      this.textBold = !this.textBold;
      document.getElementById('text-bold').classList.toggle('primary', this.textBold);
    });

    document.getElementById('text-italic').addEventListener('click', () => {
      this.textItalic = !this.textItalic;
      document.getElementById('text-italic').classList.toggle('primary', this.textItalic);
    });

    // Add event listeners for todo bold and italic buttons
    document.getElementById('todo-bold').addEventListener('click', () => {
      this.todoBold = !this.todoBold;
      document.getElementById('todo-bold').classList.toggle('primary', this.todoBold);
    });

    document.getElementById('todo-italic').addEventListener('click', () => {
      this.todoItalic = !this.todoItalic;
      document.getElementById('todo-italic').classList.toggle('primary', this.todoItalic);
    });

    document.getElementById('todo-color').addEventListener('change', (e) => {
      this.todoColor = e.target.value;
    });

    document.getElementById('toggle-todo-delete-btn').addEventListener('click', () => {
      this.showTodoDeleteButtons = !this.showTodoDeleteButtons;
      document.getElementById('toggle-todo-delete-btn').classList.toggle('primary', this.showTodoDeleteButtons);
      // Redraw all todo items with the new visibility state
      this.redrawAll();
    });

    // Add undo/redo button listeners
    document.getElementById('undo-btn').addEventListener('click', () => this.undo());
    document.getElementById('redo-btn').addEventListener('click', () => this.redo());

    this.canvas.addEventListener('mousedown', this.startPaint);
    this.canvas.addEventListener('mousemove', this.paint);
    this.canvas.addEventListener('mouseup', this.endPaint);
    this.canvas.addEventListener('mouseleave', this.endPaint);
    this.canvas.addEventListener('click', this.handleCanvasClick);

    // Touch support
    this.canvas.addEventListener('touchstart', this.onTouchStart);
    this.canvas.addEventListener('touchmove', this.onTouchMove);
    this.canvas.addEventListener('touchend', this.onTouchEnd);

    // Keyboard shortcuts for undo/redo
    document.addEventListener('keydown', this.handleKeyboard);

    // Mouse move for brush cursor
    this.canvas.addEventListener('mouseenter', this.updateBrushCursor);
    this.canvas.addEventListener('mousemove', this.updateBrushCursor);

    // Create brush cursor element
    this.createBrushCursor();

    // Initialize history with blank canvas state
    this.saveToHistory();
  }

  handleKeyboard(e) {
    // Ctrl+Z or Cmd+Z for undo
    if ((e.ctrlKey || e.metaKey) && e.key === 'z' && !e.shiftKey) {
      e.preventDefault();
      this.undo();
    }
    // Ctrl+Y or Ctrl+Shift+Z or Cmd+Shift+Z for redo
    else if ((e.ctrlKey || e.metaKey) && (e.key === 'y' || (e.shiftKey && e.key === 'z'))) {
      e.preventDefault();
      this.redo();
    }
  }

  setActiveTool(tool, title) {
    setCanvasTitle(title);
    this.currentTool = tool;

    this.canvas.parentNode.classList.toggle('brush-mode', this.currentTool === 'brush');
    this.canvas.parentNode.classList.toggle('eraser-mode', this.currentTool === 'eraser');
    this.canvas.parentNode.classList.toggle('text-mode', this.currentTool === 'text');
    this.canvas.parentNode.classList.toggle('todo-mode', this.currentTool === 'todo');
    this.canvas.parentNode.classList.toggle('schedule-mode', this.currentTool === 'schedule');

    document.getElementById('brush-mode').classList.toggle('active', this.currentTool === 'brush');
    document.getElementById('eraser-mode').classList.toggle('active', this.currentTool === 'eraser');
    document.getElementById('text-mode').classList.toggle('active', this.currentTool === 'text');
    document.getElementById('todo-mode').classList.toggle('active', this.currentTool === 'todo');
    document.getElementById('schedule-mode').classList.toggle('active', this.currentTool === 'schedule');

    document.getElementById('brush-color').disabled = this.currentTool === 'eraser' || this.currentTool === 'todo' || this.currentTool === 'schedule';
    document.getElementById('brush-size').disabled = this.currentTool === 'text' || this.currentTool === 'todo' || this.currentTool === 'schedule';

    document.getElementById('undo-btn').classList.toggle('hide', this.currentTool === null);
    document.getElementById('redo-btn').classList.toggle('hide', this.currentTool === null);

    // Cancel any pending text placement
    this.cancelTextPlacement();
  }

  createBrushCursor() {
    // Create a div element to show as brush cursor
    this.brushCursor = document.createElement('div');
    this.brushCursor.id = 'brush-cursor';
    this.brushCursor.style.position = 'fixed';
    this.brushCursor.style.border = '2px solid rgba(0, 0, 0, 0.5)';
    this.brushCursor.style.borderRadius = '50%';
    this.brushCursor.style.pointerEvents = 'none';
    this.brushCursor.style.display = 'none';
    this.brushCursor.style.zIndex = '10000';
    this.brushCursor.style.transform = 'translate(-50%, -50%)';
    this.brushCursor.style.willChange = 'transform';
    this.brushCursor.style.left = '0';
    this.brushCursor.style.top = '0';
    document.body.appendChild(this.brushCursor);
    this.updateBrushCursorSize();

    // For requestAnimationFrame throttling
    this.cursorUpdateScheduled = false;
    this.pendingCursorX = 0;
    this.pendingCursorY = 0;
  }

  updateBrushCursorSize() {
    if (!this.brushCursor) return;

    const rect = this.canvas.getBoundingClientRect();
    const scaleX = rect.width / this.canvas.width;
    const scaleY = rect.height / this.canvas.height;
    const scale = Math.min(scaleX, scaleY);

    const size = this.brushSize * scale;
    this.brushCursor.style.width = size + 'px';
    this.brushCursor.style.height = size + 'px';
  }

  updateBrushCursor(e) {
    if (!this.brushCursor) return;

    if (this.currentTool === 'brush' || this.currentTool === 'eraser') {
      // Check if mouse is within canvas bounds
      const rect = this.canvas.getBoundingClientRect();
      const isInCanvas = e.clientX >= rect.left && 
                         e.clientX <= rect.right && 
                         e.clientY >= rect.top && 
                         e.clientY <= rect.bottom;

      if (isInCanvas) {
        this.brushCursor.style.display = 'block';
        this.canvas.style.cursor = 'none';

        // Store the pending position
        this.pendingCursorX = e.clientX;
        this.pendingCursorY = e.clientY;

        // Schedule update using requestAnimationFrame for smooth movement
        if (!this.cursorUpdateScheduled) {
          this.cursorUpdateScheduled = true;
          requestAnimationFrame(() => {
            this.brushCursor.style.transform = `translate(${this.pendingCursorX}px, ${this.pendingCursorY}px) translate(-50%, -50%)`;
            this.cursorUpdateScheduled = false;
          });
        }

        // Update color to match brush or show white for eraser (only needs to happen once or when tool changes)
        if (this.currentTool === 'eraser') {
          if (this.brushCursor.getAttribute('data-tool') !== 'eraser') {
            this.brushCursor.style.border = '2px solid rgba(255, 0, 0, 0.7)';
            this.brushCursor.style.backgroundColor = 'rgba(255, 255, 255, 0.2)';
            this.brushCursor.style.boxShadow = 'none';
            this.brushCursor.setAttribute('data-tool', 'eraser');
          }
        } else {
          if (this.brushCursor.getAttribute('data-tool') !== 'brush') {
            // Use a contrasting border - white with black outline for visibility
            this.brushCursor.style.border = '1px solid white';
            this.brushCursor.style.boxShadow = '0 0 0 1px black, inset 0 0 0 1px black';
            this.brushCursor.style.backgroundColor = 'transparent';
            this.brushCursor.setAttribute('data-tool', 'brush');
          }
        }
      } else {
        // Hide cursor when outside canvas
        this.brushCursor.style.display = 'none';
      }
    }
  }

  hideBrushCursor() {
    if (this.brushCursor) {
      this.brushCursor.style.display = 'none';
    }
    this.canvas.style.cursor = 'default';
  }

  startPaint(e) {
    if (!this.currentTool) return;

    if (this.currentTool === 'text') {
      // Check if we're clicking on a text element to drag
      const textElement = this.findTextElementAt(e);
      if (textElement) {
        this.isDraggingText = true;
        this.selectedTextElement = textElement;

        // Save current canvas state for dragging
        this.draggingCanvasContext = this.ctx.getImageData(0, 0, this.canvas.width, this.canvas.height);

        const rect = this.canvas.getBoundingClientRect();
        const scaleX = this.canvas.width / rect.width;
        const scaleY = this.canvas.height / rect.height;
        const x = (e.clientX - rect.left) * scaleX;
        const y = (e.clientY - rect.top) * scaleY;

        // Calculate offset for smooth dragging
        this.dragOffsetX = textElement.x - x;
        this.dragOffsetY = textElement.y - y;

        return; // Don't start drawing
      }
    } else if (this.currentTool === 'todo') {
      // Check if we're clicking on a delete button first
      const deleteButtonTodo = this.findTodoDeleteButtonAt(e);
      if (deleteButtonTodo) {
        this.deleteTodoItem(deleteButtonTodo);
        return;
      }

      // Check if we're clicking on a todo item to drag
      const todoItem = this.findTodoItemAt(e);
      if (todoItem) {
        this.isDraggingText = true;
        this.selectedTodoItem = todoItem;

        // Save current canvas state for dragging
        this.draggingCanvasContext = this.ctx.getImageData(0, 0, this.canvas.width, this.canvas.height);

        const rect = this.canvas.getBoundingClientRect();
        const scaleX = this.canvas.width / rect.width;
        const scaleY = this.canvas.height / rect.height;
        const x = (e.clientX - rect.left) * scaleX;
        const y = (e.clientY - rect.top) * scaleY;

        // Calculate offset for smooth dragging
        this.dragOffsetX = todoItem.x - x;
        this.dragOffsetY = todoItem.y - y;

        return; // Don't place new todo
      }
    } else if (this.currentTool === 'schedule') {
      // Schedule mode - don't paint, handle through handleCanvasClick
      return;
    } else {
      this.painting = true;
      this.draw(e);
    }
  }

  endPaint() {
    if (this.isDraggingText) {
      // After dragging text or todo, redraw all elements to clean up old positions
      this.redrawAll();
      this.saveToHistory(); // Save state after dragging
    } else if (this.painting) {
      this.saveToHistory(); // Save state after drawing
    }
    this.painting = false;
    this.isDraggingText = false;
    this.lastX = 0;
    this.lastY = 0;

    this.hideBrushCursor();
  }

  paint(e) {
    if (!this.currentTool) return;

    if (this.currentTool === 'text') {
      if (this.isDraggingText && this.selectedTextElement) {
        this.dragText(e);
      }
    } else if (this.currentTool === 'todo') {
      if (this.isDraggingText && this.selectedTodoItem) {
        this.dragTodo(e);
      }
    } else {
      if (this.painting) {
        this.draw(e);
      }
    }
  }

  draw(e) {
    const rect = this.canvas.getBoundingClientRect();
    const scaleX = this.canvas.width / rect.width;
    const scaleY = this.canvas.height / rect.height;
    const x = (e.clientX - rect.left) * scaleX;
    const y = (e.clientY - rect.top) * scaleY;

    this.ctx.lineJoin = 'round';
    this.ctx.lineCap = 'round';
    this.ctx.strokeStyle = this.brushColor;
    this.ctx.lineWidth = this.brushSize;

    this.ctx.beginPath();

    if (this.lastX === 0 && this.lastY === 0) {
      // For the first point, just do a dot
      this.ctx.moveTo(x, y);
      this.ctx.lineTo(x + 0.1, y + 0.1);

      // Store the dot for redrawing
      this.lineSegments.push({
        type: 'dot',
        x: x,
        y: y,
        color: this.brushColor,
        size: this.brushSize
      });
    } else {
      // Connect to the previous point
      this.ctx.moveTo(this.lastX, this.lastY);
      this.ctx.lineTo(x, y);

      // Store the line segment for redrawing
      this.lineSegments.push({
        type: 'line',
        x1: this.lastX,
        y1: this.lastY,
        x2: x,
        y2: y,
        color: this.brushColor,
        size: this.brushSize
      });
    }

    this.ctx.stroke();

    this.lastX = x;
    this.lastY = y;
  }

  handleCanvasClick(e) {
    if (this.currentTool === 'text' && this.isTextPlacementMode) {
      this.placeText(e);
    } else if (this.currentTool === 'todo' && this.isTodoPlacementMode) {
      this.placeTodo(e);
    } else if (this.currentTool === 'schedule') {
      // Handle schedule cell click for editing
      const cell = this.getScheduleCellAt(e);
      if (cell) {
        this.selectedScheduleCell = cell;
        const currentText = this.scheduleData[cell.row][cell.col];
        document.getElementById('schedule-input').value = currentText;
        document.getElementById('schedule-input').focus();
        // Show the input buttons by making the second flex-group visible
        const allScheduleTools = document.querySelectorAll('.schedule-tools');
        if (allScheduleTools.length > 1) {
          allScheduleTools[1].style.display = 'flex';
        }
      }
    }
  }

  onTouchStart(e) {
    e.preventDefault();
    const touch = e.touches[0];

    // If in text placement mode, handle as a click
    if (this.currentTool === 'text' && this.isTextPlacementMode) {
      const mouseEvent = new MouseEvent('click', {
        clientX: touch.clientX,
        clientY: touch.clientY
      });
      this.canvas.dispatchEvent(mouseEvent);
      return;
    }

    // Otherwise handle as normal drawing
    const mouseEvent = new MouseEvent('mousedown', {
      clientX: touch.clientX,
      clientY: touch.clientY
    });
    this.canvas.dispatchEvent(mouseEvent);
  }

  onTouchMove(e) {
    e.preventDefault();
    const touch = e.touches[0];
    const mouseEvent = new MouseEvent('mousemove', {
      clientX: touch.clientX,
      clientY: touch.clientY
    });
    this.canvas.dispatchEvent(mouseEvent);
  }

  onTouchEnd(e) {
    e.preventDefault();
    this.endPaint();
  }

  dragText(e) {
    const rect = this.canvas.getBoundingClientRect();
    const scaleX = this.canvas.width / rect.width;
    const scaleY = this.canvas.height / rect.height;
    const x = (e.clientX - rect.left) * scaleX;
    const y = (e.clientY - rect.top) * scaleY;

    // Update text position with offset
    this.selectedTextElement.x = x + this.dragOffsetX;
    this.selectedTextElement.y = y + this.dragOffsetY;

    // Restore the saved canvas state (with all other elements)
    if (this.draggingCanvasContext) {
      this.ctx.putImageData(this.draggingCanvasContext, 0, 0);
    }

    // Redraw all other text elements (except the one being dragged)
    this.textElements.forEach(item => {
      if (item !== this.selectedTextElement) {
        this.ctx.font = item.font;
        this.ctx.fillStyle = item.color;
        this.ctx.fillText(item.text, item.x, item.y);
      }
    });

    // Redraw all todo items
    this.redrawTodoItems();

    // Draw the dragged text element on top
    this.ctx.font = this.selectedTextElement.font;
    this.ctx.fillStyle = this.selectedTextElement.color;
    this.ctx.fillText(this.selectedTextElement.text, this.selectedTextElement.x, this.selectedTextElement.y);
  }

  dragTodo(e) {
    const rect = this.canvas.getBoundingClientRect();
    const scaleX = this.canvas.width / rect.width;
    const scaleY = this.canvas.height / rect.height;
    const x = (e.clientX - rect.left) * scaleX;
    const y = (e.clientY - rect.top) * scaleY;

    // Update todo position with offset
    this.selectedTodoItem.x = x + this.dragOffsetX;
    this.selectedTodoItem.y = y + this.dragOffsetY;

    // Restore the saved canvas state (with all other elements)
    if (this.draggingCanvasContext) {
      this.ctx.putImageData(this.draggingCanvasContext, 0, 0);
    }

    // Redraw all other todo items (except the one being dragged)
    this.todoItems.forEach(item => {
      if (item !== this.selectedTodoItem) {
        this.drawTodoItem(item);
      }
    });

    // Draw the dragged todo item on top
    this.drawTodoItem(this.selectedTodoItem);
  }

  findTextElementAt(e) {
    const rect = this.canvas.getBoundingClientRect();
    const scaleX = this.canvas.width / rect.width;
    const scaleY = this.canvas.height / rect.height;
    const x = (e.clientX - rect.left) * scaleX;
    const y = (e.clientY - rect.top) * scaleY;

    // Search through text elements in reverse order (top-most first)
    for (let i = this.textElements.length - 1; i >= 0; i--) {
      const text = this.textElements[i];

      // Calculate text dimensions
      this.ctx.font = text.font;
      const textWidth = this.ctx.measureText(text.text).width;

      // Extract font size correctly from the font string
      const fontSizeMatch = text.font.match(/(\d+)px/);
      const fontSize = fontSizeMatch ? parseInt(fontSizeMatch[1]) : 14;
      const textHeight = fontSize * 1.2; // Approximate height

      // Check if click is within text bounds (allowing for some margin)
      const margin = 5;
      if (x >= text.x - margin &&
        x <= text.x + textWidth + margin &&
        y >= text.y - textHeight + margin &&
        y <= text.y + margin) {
        return text;
      }
    }

    return null;
  }

  findTodoItemAt(e) {
    const rect = this.canvas.getBoundingClientRect();
    const scaleX = this.canvas.width / rect.width;
    const scaleY = this.canvas.height / rect.height;
    const x = (e.clientX - rect.left) * scaleX;
    const y = (e.clientY - rect.top) * scaleY;

    // Search through todo items in reverse order (top-most first)
    for (let i = this.todoItems.length - 1; i >= 0; i--) {
      const todo = this.todoItems[i];

      // Calculate text dimensions
      this.ctx.font = todo.font;
      const textWidth = this.ctx.measureText(todo.text).width;

      // Extract font size correctly from the font string
      const fontSizeMatch = todo.font.match(/(\d+)px/);
      const fontSize = fontSizeMatch ? parseInt(fontSizeMatch[1]) : 14;
      const textHeight = fontSize * 1.2; // Approximate height

      // Check if click is within todo bounds (allowing for some margin)
      const margin = 5;
      if (x >= todo.x - margin &&
        x <= todo.x + textWidth + margin &&
        y >= todo.y - textHeight + margin &&
        y <= todo.y + margin) {
        return todo;
      }
    }

    return null;
  }

  findTodoDeleteButtonAt(e) {
    const rect = this.canvas.getBoundingClientRect();
    const scaleX = this.canvas.width / rect.width;
    const scaleY = this.canvas.height / rect.height;
    const x = (e.clientX - rect.left) * scaleX;
    const y = (e.clientY - rect.top) * scaleY;

    // Search through todo items in reverse order (top-most first)
    for (let i = this.todoItems.length - 1; i >= 0; i--) {
      const todo = this.todoItems[i];

      if (!todo.deleteButtonX || !todo.deleteButtonY || !todo.deleteButtonSize) {
        continue;
      }

      // Check if click is within delete button bounds
      if (x >= todo.deleteButtonX - 5 &&
        x <= todo.deleteButtonX + 10 &&
        y >= todo.deleteButtonY &&
        y <= todo.deleteButtonY + todo.deleteButtonSize) {
        return todo;
      }
    }

    return null;
  }

  deleteTodoItem(todoItem) {
    const index = this.todoItems.indexOf(todoItem);
    if (index > -1) {
      this.todoItems.splice(index, 1);
      // Redraw canvas to remove the deleted todo item
      this.redrawAll();
      this.saveToHistory();
    }
  }

  redrawAll() {
    // Clear canvas to white background
    this.ctx.fillStyle = 'white';
    this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);

    // Redraw all elements in order
    this.redrawLineSegments();
    this.redrawTextElements();
    this.redrawTodoItems();
    this.drawSchedule(); // Redraw schedule if it exists
  }

  startTextPlacement() {
    const text = document.getElementById('text-input').value.trim();
    if (!text) {
      alert('请输入文字内容');
      return;
    }

    this.isTextPlacementMode = true;

    // Add visual feedback
    setCanvasTitle('点击画布放置文字');
    this.canvas.classList.add('text-placement-mode');
  }

  cancelTextPlacement() {
    this.isTextPlacementMode = false;
    this.isTodoPlacementMode = false;
    this.canvas.classList.remove('text-placement-mode');

    // reset dragging state
    this.isDraggingText = false;
    this.dragOffsetX = 0;
    this.dragOffsetY = 0;
    this.selectedTextElement = null;
    this.selectedTodoItem = null;
    this.draggingCanvasContext = null;
  }

  placeText(e) {
    const rect = this.canvas.getBoundingClientRect();
    const scaleX = this.canvas.width / rect.width;
    const scaleY = this.canvas.height / rect.height;
    const x = (e.clientX - rect.left) * scaleX;
    const y = (e.clientY - rect.top) * scaleY;

    const text = document.getElementById('text-input').value;
    const fontFamily = document.getElementById('font-family').value;
    const fontSize = document.getElementById('font-size').value;

    // Build font style string
    let fontStyle = '';
    if (this.textItalic) fontStyle += 'italic ';
    if (this.textBold) fontStyle += 'bold ';

    // Create a new text element
    const newText = {
      text: text,
      x: x,
      y: y,
      font: `${fontStyle}${fontSize}px ${fontFamily}`,
      color: this.brushColor
    };

    // Add to our list of text elements
    this.textElements.push(newText);

    // Select this text element for immediate dragging
    this.selectedTextElement = newText;
    this.draggingCanvasContext = this.ctx.getImageData(0, 0, this.canvas.width, this.canvas.height);

    // Draw text on canvas
    this.ctx.font = newText.font;
    this.ctx.fillStyle = newText.color;
    this.ctx.fillText(newText.text, newText.x, newText.y);

    // Save to history after placing text
    this.saveToHistory();

    // Reset
    document.getElementById('text-input').value = '';
    this.isTextPlacementMode = false;
    this.canvas.classList.remove('text-placement-mode');
    setCanvasTitle('拖动新添加文字可调整位置');
  }

  redrawTextElements() {
    // Redraw all text elements after dithering
    this.textElements.forEach(item => {
      this.ctx.font = item.font;
      this.ctx.fillStyle = item.color;
      this.ctx.fillText(item.text, item.x, item.y);
    });
  }

  redrawLineSegments() {
    // Redraw all line segments after dithering
    this.lineSegments.forEach(segment => {
      this.ctx.lineJoin = 'round';
      this.ctx.lineCap = 'round';
      this.ctx.strokeStyle = segment.color;
      this.ctx.lineWidth = segment.size;
      this.ctx.beginPath();

      if (segment.type === 'dot') {
        this.ctx.moveTo(segment.x, segment.y);
        this.ctx.lineTo(segment.x + 0.1, segment.y + 0.1);
      } else {
        this.ctx.moveTo(segment.x1, segment.y1);
        this.ctx.lineTo(segment.x2, segment.y2);
      }

      this.ctx.stroke();
    });
  }

  startTodoPlacement() {
    const todo = document.getElementById('todo-input').value.trim();
    if (!todo) {
      alert('请输入待办项内容');
      return;
    }

    this.isTodoPlacementMode = true;

    // Add visual feedback
    setCanvasTitle('点击画布放置待办项');
    this.canvas.classList.add('text-placement-mode');
  }

  placeTodo(e) {
    const rect = this.canvas.getBoundingClientRect();
    const scaleX = this.canvas.width / rect.width;
    const scaleY = this.canvas.height / rect.height;
    const x = (e.clientX - rect.left) * scaleX;
    const y = (e.clientY - rect.top) * scaleY;

    const todo = document.getElementById('todo-input').value;
    const fontSize = document.getElementById('todo-font-size').value;
    const fontFamily = document.getElementById('todo-font-family').value;

    // Build font style string with bold and italic
    let fontStyle = '';
    if (this.todoItalic) fontStyle += 'italic ';
    if (this.todoBold) fontStyle += 'bold ';
    fontStyle += `${fontSize}px ${fontFamily}`;

    // Create a new todo item
    const newTodo = {
      text: todo,
      x: x,
      y: y,
      font: fontStyle,
      color: this.todoColor,
      completed: false
    };

    // Add to our list of todo items
    this.todoItems.push(newTodo);

    // Select this todo item for immediate dragging
    this.selectedTodoItem = newTodo;
    this.draggingCanvasContext = this.ctx.getImageData(0, 0, this.canvas.width, this.canvas.height);

    // Draw todo on canvas
    this.drawTodoItem(newTodo);

    // Save to history after placing todo
    this.saveToHistory();

    // Reset
    document.getElementById('todo-input').value = '';
    this.isTodoPlacementMode = false;
    this.canvas.classList.remove('text-placement-mode');
    setCanvasTitle('拖动新添加待办项可调整位置');
  }

  drawTodoItem(todoItem) {
    // Draw todo text
    this.ctx.font = todoItem.font;
    this.ctx.fillStyle = todoItem.color;
    this.ctx.fillText(todoItem.text, todoItem.x, todoItem.y);

    // Only draw delete button if showTodoDeleteButtons is true
    if (this.showTodoDeleteButtons) {
      // Calculate delete button position
      const textWidth = this.ctx.measureText(todoItem.text).width;
      const deleteButtonX = todoItem.x + textWidth + 5;
      const deleteButtonY = todoItem.y;
      const deleteButtonSize = 12;

      // Draw delete button "×"
      this.ctx.font = 'bold 14px Arial';
      this.ctx.fillStyle = '#FF6B6B';
      this.ctx.fillText('×', deleteButtonX, deleteButtonY);

      // Store delete button coordinates for hit detection
      todoItem.deleteButtonX = deleteButtonX;
      todoItem.deleteButtonY = deleteButtonY - deleteButtonSize;
      todoItem.deleteButtonSize = deleteButtonSize + 5;
    }

    // Draw strikethrough if completed
    if (todoItem.completed) {
      const textWidth = this.ctx.measureText(todoItem.text).width;
      const strikeY = todoItem.y - 4;
      this.ctx.strokeStyle = '#000000';
      this.ctx.lineWidth = 1;
      this.ctx.beginPath();
      this.ctx.moveTo(todoItem.x, strikeY);
      this.ctx.lineTo(todoItem.x + textWidth, strikeY);
      this.ctx.stroke();
    }
  }

  redrawTodoItems() {
    // Redraw all todo items
    this.todoItems.forEach(item => {
      this.drawTodoItem(item);
    });
  }

  createSchedule() {
    // Get schedule configuration from form inputs
    this.scheduleDays = parseInt(document.getElementById('schedule-days').value);
    this.scheduleClasses = parseInt(document.getElementById('schedule-classes').value);
    this.scheduleFontFamily = document.getElementById('schedule-font-family').value;
    this.scheduleFontSize = parseInt(document.getElementById('schedule-font-size').value);
    this.scheduleColor = document.getElementById('schedule-color').value;

    // Clear old content (drawings, text, todos) to make room for new schedule
    this.lineSegments = [];
    this.textElements = [];
    this.todoItems = [];

    // Calculate cell dimensions based on canvas
    this.calculateScheduleDimensions();

    // Initialize schedule data (2D array: rows = classes + 1 (header), cols = days + 1 (time col))
    this.scheduleData = [];
    for (let i = 0; i <= this.scheduleClasses; i++) {
      this.scheduleData[i] = [];
      for (let j = 0; j <= this.scheduleDays; j++) {
        if (i === 0 && j === 0) {
          this.scheduleData[i][j] = ''; // Top-left corner - leave empty
        } else if (i === 0) {
          this.scheduleData[i][j] = this.weekDays[j - 1]; // Header row - weekdays
        } else if (j === 0) {
          this.scheduleData[i][j] = `第${i}节`; // Time column
        } else {
          this.scheduleData[i][j] = ''; // Empty cells for courses
        }
      }
    }

    // Draw the schedule on canvas
    this.redrawAll();
    this.saveScheduleToLocalStorage(); // Save schedule to cache
    this.saveToHistory();
  }

  calculateScheduleDimensions() {
    // Calculate cell size based on canvas dimensions
    const padding = 20;
    const availableWidth = this.canvas.width - 2 * padding;
    const availableHeight = this.canvas.height - 2 * padding;

    // Calculate cell dimensions to fit content
    const cellWidth = Math.floor(availableWidth / (this.scheduleDays + 1)); // +1 for time column
    const cellHeight = Math.floor(availableHeight / (this.scheduleClasses + 1)); // +1 for header

    // Ensure cells are large enough for text
    this.scheduleCellWidth = Math.max(cellWidth, this.scheduleFontSize * 4);
    this.scheduleCellHeight = Math.max(cellHeight, this.scheduleFontSize * 2);

    // Adjust start position based on available space
    this.scheduleStartX = padding;
    this.scheduleStartY = padding;
  }

  drawSchedule() {
    if (!this.scheduleData) return;

    const cellWidth = this.scheduleCellWidth;
    const cellHeight = this.scheduleCellHeight;
    const startX = this.scheduleStartX;
    const startY = this.scheduleStartY;
    const font = `${this.scheduleFontSize}px ${this.scheduleFontFamily}`;

    this.ctx.font = font;
    this.ctx.fillStyle = this.scheduleColor;
    this.ctx.strokeStyle = '#000000';
    this.ctx.lineWidth = 1;

    // Draw table grid and text
    for (let i = 0; i < this.scheduleData.length; i++) {
      for (let j = 0; j < this.scheduleData[i].length; j++) {
        const x = startX + j * cellWidth;
        const y = startY + i * cellHeight;

        // Draw cell border
        this.ctx.strokeRect(x, y, cellWidth, cellHeight);

        // Draw cell text
        const text = this.scheduleData[i][j];
        if (text) {
          const textX = x + cellWidth / 2 - this.ctx.measureText(text).width / 2;
          const textY = y + cellHeight / 2 + this.scheduleFontSize / 3;
          this.ctx.fillText(text, textX, textY);
        }
      }
    }

    // Draw selection indicator if enabled and a cell is selected
    if (this.showScheduleCellIndicator && this.selectedScheduleCell) {
      const row = this.selectedScheduleCell.row;
      const col = this.selectedScheduleCell.col;
      const x = startX + col * cellWidth;
      const y = startY + row * cellHeight;

      // Draw a small black dot in the top-right corner of selected cell
      this.ctx.fillStyle = '#000000';
      this.ctx.beginPath();
      this.ctx.arc(x + cellWidth - 5, y + 5, 3, 0, Math.PI * 2);
      this.ctx.fill();
    }
  }

  updateScheduleCell(row, col, text) {
    if (this.scheduleData && row >= 0 && col >= 0 && row <= this.scheduleClasses && col <= this.scheduleDays) {
      this.scheduleData[row][col] = text;
      this.redrawAll();
      this.saveScheduleToLocalStorage(); // Save schedule to cache
      this.saveToHistory();
    }
  }

  getScheduleCellAt(e) {
    if (!this.scheduleData) return null;

    const rect = this.canvas.getBoundingClientRect();
    const scaleX = this.canvas.width / rect.width;
    const scaleY = this.canvas.height / rect.height;
    const x = (e.clientX - rect.left) * scaleX;
    const y = (e.clientY - rect.top) * scaleY;

    const cellWidth = this.scheduleCellWidth;
    const cellHeight = this.scheduleCellHeight;
    const startX = this.scheduleStartX;
    const startY = this.scheduleStartY;

    // Calculate which cell was clicked
    const col = Math.floor((x - startX) / cellWidth);
    const row = Math.floor((y - startY) / cellHeight);

    // Check if click is within schedule bounds - allow all cells including header row and time column
    if (col >= 0 && row >= 0 && col <= this.scheduleDays && row <= this.scheduleClasses) {
      return { row, col };
    }

    return null;
  }

  confirmScheduleInput() {
    if (this.selectedScheduleCell) {
      const text = document.getElementById('schedule-input').value;
      this.updateScheduleCell(this.selectedScheduleCell.row, this.selectedScheduleCell.col, text);
      this.cancelScheduleInput();
    }
  }

  cancelScheduleInput() {
    this.selectedScheduleCell = null;
    document.getElementById('schedule-input').value = '';
    // Hide the input buttons
    const allScheduleTools = document.querySelectorAll('.schedule-tools');
    if (allScheduleTools.length > 1) {
      allScheduleTools[1].style.display = 'none';
    }
    // Redraw to remove the selection indicator
    if (this.scheduleData) {
      this.redrawAll();
    }
  }

  saveScheduleToLocalStorage() {
    try {
      const scheduleCache = {
        scheduleData: this.scheduleData,
        scheduleDays: this.scheduleDays,
        scheduleClasses: this.scheduleClasses,
        scheduleFontFamily: this.scheduleFontFamily,
        scheduleFontSize: this.scheduleFontSize,
        scheduleColor: this.scheduleColor,
        scheduleCellWidth: this.scheduleCellWidth,
        scheduleCellHeight: this.scheduleCellHeight,
        scheduleStartX: this.scheduleStartX,
        scheduleStartY: this.scheduleStartY
      };
      localStorage.setItem('scheduleCache', JSON.stringify(scheduleCache));
    } catch (e) {
      console.error('Failed to save schedule to localStorage:', e);
    }
  }

  loadScheduleFromLocalStorage() {
    try {
      const savedData = localStorage.getItem('scheduleCache');
      if (!savedData) return false;

      const scheduleCache = JSON.parse(savedData);

      // Restore schedule configuration
      this.scheduleData = scheduleCache.scheduleData;
      this.scheduleDays = scheduleCache.scheduleDays;
      this.scheduleClasses = scheduleCache.scheduleClasses;
      this.scheduleFontFamily = scheduleCache.scheduleFontFamily;
      this.scheduleFontSize = scheduleCache.scheduleFontSize;
      this.scheduleColor = scheduleCache.scheduleColor;

      // Recalculate dimensions based on current canvas size for proper adaptation
      this.calculateScheduleDimensions();

      if (this.scheduleData) {
        this.drawSchedule();
        return true;
      }
      return false;
    } catch (e) {
      console.error('Failed to load schedule from localStorage:', e);
      return false;
    }
  }

  clearScheduleCache() {
    try {
      localStorage.removeItem('scheduleCache');
    } catch (e) {
      console.error('Failed to clear schedule cache:', e);
    }
  }

  clearElements() {
    this.textElements = [];
    this.lineSegments = [];
    this.todoItems = [];
    this.scheduleData = null; // Clear schedule data
  }
}
