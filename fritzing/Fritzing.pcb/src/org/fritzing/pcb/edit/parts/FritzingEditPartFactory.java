/*
 * (c) Fachhochschule Potsdam
 */
package org.fritzing.pcb.edit.parts;

import org.eclipse.draw2d.FigureUtilities;
import org.eclipse.draw2d.Label;
import org.eclipse.draw2d.geometry.Dimension;
import org.eclipse.draw2d.geometry.Rectangle;
import org.eclipse.gef.EditPart;
import org.eclipse.gef.EditPartFactory;
import org.eclipse.gef.tools.CellEditorLocator;
import org.eclipse.gmf.runtime.diagram.ui.editparts.ITextAwareEditPart;
import org.eclipse.gmf.runtime.draw2d.ui.figures.WrapLabel;
import org.eclipse.gmf.runtime.notation.View;
import org.eclipse.jface.viewers.CellEditor;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Text;
import org.fritzing.pcb.part.FritzingVisualIDRegistry;

/**
 * @generated
 */
public class FritzingEditPartFactory implements EditPartFactory {

	/**
	 * @generated
	 */
	public EditPart createEditPart(EditPart context, Object model) {
		if (model instanceof View) {
			View view = (View) model;
			switch (FritzingVisualIDRegistry.getVisualID(view)) {

			case SketchEditPart.VISUAL_ID:
				return new SketchEditPart(view);

			case LEDEditPart.VISUAL_ID:
				return new LEDEditPart(view);

			case LEDNameEditPart.VISUAL_ID:
				return new LEDNameEditPart(view);

			case ResistorEditPart.VISUAL_ID:
				return new ResistorEditPart(view);

			case ResistorNameEditPart.VISUAL_ID:
				return new ResistorNameEditPart(view);

			case TerminalEditPart.VISUAL_ID:
				return new TerminalEditPart(view);

			case TerminalNameEditPart.VISUAL_ID:
				return new TerminalNameEditPart(view);

			case GenericPartEditPart.VISUAL_ID:
				return new GenericPartEditPart(view);

			case GenericPartNameEditPart.VISUAL_ID:
				return new GenericPartNameEditPart(view);

			case Terminal2EditPart.VISUAL_ID:
				return new Terminal2EditPart(view);

			case TerminalName2EditPart.VISUAL_ID:
				return new TerminalName2EditPart(view);

			case WireEditPart.VISUAL_ID:
				return new WireEditPart(view);

			case WireNameEditPart.VISUAL_ID:
				return new WireNameEditPart(view);

			case TrackEditPart.VISUAL_ID:
				return new TrackEditPart(view);

			case LegEditPart.VISUAL_ID:
				return new LegEditPart(view);
			}
		}
		return createUnrecognizedEditPart(context, model);
	}

	/**
	 * @generated
	 */
	private EditPart createUnrecognizedEditPart(EditPart context, Object model) {
		// Handle creation of unrecognized child node EditParts here
		return null;
	}

	/**
	 * @generated
	 */
	public static CellEditorLocator getTextCellEditorLocator(
			ITextAwareEditPart source) {
		if (source.getFigure() instanceof WrapLabel)
			return new TextCellEditorLocator((WrapLabel) source.getFigure());
		else {
			return new LabelCellEditorLocator((Label) source.getFigure());
		}
	}

	/**
	 * @generated
	 */
	static private class TextCellEditorLocator implements CellEditorLocator {

		/**
		 * @generated
		 */
		private WrapLabel wrapLabel;

		/**
		 * @generated
		 */
		public TextCellEditorLocator(WrapLabel wrapLabel) {
			this.wrapLabel = wrapLabel;
		}

		/**
		 * @generated
		 */
		public WrapLabel getWrapLabel() {
			return wrapLabel;
		}

		/**
		 * @generated NOT
		 * 
		 * jrc--added a try/catch around the generated code
		 * as an uncaught exception here
		 * seems to kill drag feedback thereafter (mac-only bug)
		 * 
		 */
		public void relocate(CellEditor celleditor) {
			try {
				Text text = (Text) celleditor.getControl();
				Rectangle rect = getWrapLabel().getTextBounds().getCopy();
				getWrapLabel().translateToAbsolute(rect);
				if (getWrapLabel().isTextWrapped()
						&& getWrapLabel().getText().length() > 0) {
					rect.setSize(new Dimension(text.computeSize(rect.width,
							SWT.DEFAULT)));
				} else {
					int avr = FigureUtilities.getFontMetrics(text.getFont())
							.getAverageCharWidth();
					rect.setSize(new Dimension(text.computeSize(SWT.DEFAULT,
							SWT.DEFAULT)).expand(avr * 2, 0));
				}
				if (!rect.equals(new Rectangle(text.getBounds()))) {
					text.setBounds(rect.x, rect.y, rect.width, rect.height);
				}
			}
			catch (Exception ex) {
				// don't do anything
			}
		}

	}

	/**
	 * @generated
	 */
	private static class LabelCellEditorLocator implements CellEditorLocator {

		/**
		 * @generated
		 */
		private Label label;

		/**
		 * @generated
		 */
		public LabelCellEditorLocator(Label label) {
			this.label = label;
		}

		/**
		 * @generated
		 */
		public Label getLabel() {
			return label;
		}

		/**
		 * @generated
		 */
		public void relocate(CellEditor celleditor) {
			Text text = (Text) celleditor.getControl();
			Rectangle rect = getLabel().getTextBounds().getCopy();
			getLabel().translateToAbsolute(rect);
			int avr = FigureUtilities.getFontMetrics(text.getFont())
					.getAverageCharWidth();
			rect.setSize(new Dimension(text.computeSize(SWT.DEFAULT,
					SWT.DEFAULT)).expand(avr * 2, 0));
			if (!rect.equals(new Rectangle(text.getBounds()))) {
				text.setBounds(rect.x, rect.y, rect.width, rect.height);
			}
		}
	}
}
