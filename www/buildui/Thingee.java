import java.awt.*;
import java.lang.Math;
import java.util.*;
import java.text.*;

class Thingee {
    private String name;
    private int x, y;

    private int stringWidth;
    private boolean stringWidthValid;

    static private Dictionary selections;

    public void select() {
	if (!isSelected()) {
	    selections.put( this, new Integer(1) );
	}
    }

    public static Enumeration selectedElements() {
	return selections.keys();
    }

    public static void deselectAll() {
	selections = new Hashtable();
    }

    public void deselect() {
	selections.remove( this );
    }

    static public int selectedCount() {
	return selections.size();
    }

    public boolean isSelected() {
	return selections.get(this) != null;
    }

    static private Dictionary names;
    
    protected Dictionary properties;

    public void copyProps( Thingee t ) {
	Enumeration e = t.properties.keys();

	while (e.hasMoreElements()) {
	    String s = (String)e.nextElement();

	    properties.put( new String(s), t.properties.get( s ) );
	}

    }

    static public String genName( String name ) {
	/*
	if (names.get( name ) == null) {
	    return name;
	}
	*/

	int num = 0;
	String n = new String();
	do {
	    n = name + NumberFormat.getInstance().format(num); 
	    num++;
	} while(names.get( n ) != null);
	return new String(n);
    }

    public synchronized String getProperty( String name, String def ) {
	if (0 == name.compareTo("name")) {
	    // note that the name _can_ be blank.
	    return this.name;
	}
	Object foo = properties.get(name);
	if (foo == null || 0 == ((String)foo).compareTo("")) { 
	    return def;
	} else { 
	    return (String)foo; 
	}
    }

    public synchronized void setProperty( String name, String value ) {
	if (0 == name.compareTo("name")) {
	    setName( new String(value) );
	} else {
	    properties.put( new String(name), new String(value) );
	}
    }

    public int getX() { return x; }
    public int getY() { return y; }

    public boolean linkable;
    public boolean moveable;
    public boolean propertyEditable;
    public boolean trashable;

    //static Thingee currentlySelected;

    static {
	names = new Hashtable();
	//currentlySelected = null;
	selections = new Hashtable();
    }
    /*
    static public Thingee getSelected() {
	return currentlySelected;
    }

    static public void setSelected( Thingee t ) {
	currentlySelected = t;
	}*/

    public String getName() {
	return name;
    }
    
    public void setName( String newName ) {
	System.out.println("Thingee.setName(): Renamed from \"" + name + 
			   "\" to \"" + newName + "\"" );
	names.remove( name );
	name = new String( newName );
	names.put( name, new Integer(1) );
	stringWidthValid = false;
    }
	
    public Thingee(String newName) {
	name = new String( newName );
	names.put( name, new Integer(1) );
	properties = new Hashtable();
	stringWidth = 128;
	stringWidthValid = false;
	linkable = true;
	moveable = true;
	propertyEditable = true;
	trashable = true;
    }

    public void move( int nx, int ny ) {
	x = nx;
	y = ny;
    }

    public int size() { return 16; }

    public boolean clicked( int cx, int cy ) {
	if (isSelected()) {
	    Rectangle r = getRectangle();
	    return (cx > r.x && 
		    cy > r.y &&
		    cx < (r.x + r.width) && 
		    cy < (r.y + r.height));
	} else {
	    return (Math.abs(cx - x) < size() && Math.abs(cy - y) < size());
	}
    }

    public Rectangle getRectangle() {
	    int wid = 26;
	    if ((stringWidth/2) > wid) { wid = (stringWidth/2) + 4; }
	    return new Rectangle( x + -wid - 2, y + -21 - 2, 
				  wid * 2 + 6, 57 + 6 );
    }

    public void drawIcon( Graphics g ) {
	g.setColor( Color.lightGray );
	g.fillRect( -12, -12, 32, 32 );

	g.setColor( Color.white );
	g.fillRect( -16, -16, 32, 32 );
 
	g.setColor( Color.black );
	g.drawRect( -16, -16, 32, 32 );	
    }

    public int textDown() {
	return 30;
    }

    public void draw( Graphics g ) {
	if (!stringWidthValid) {
	    FontMetrics fm = g.getFontMetrics();
	    stringWidth = fm.stringWidth(name);
	    stringWidthValid = true;
	}

	g.translate( x, y );

	if (isSelected()) {
	    Rectangle r = getRectangle();
	    g.setColor( Color.white );
	    g.fillRect( r.x - x, r.y - y, r.width, r.height );	    

	    g.setColor( Color.black );
	    g.drawRect( r.x - x , r.y - y, r.width - 2, r.height - 2);	    
	    g.drawRect( r.x - x + 1, r.y - y + 1, r.width - 4, r.height - 4);	    
	    /*
	    boolean anytext = (name != "");
	    int wid = 26;
	    if ((stringWidth/2) > wid) { wid = (stringWidth/2) + 4; }
	    g.setColor( Color.white );
	    g.fillRect( -wid, -21, wid * 2, anytext ? 57 : 42 );

	    g.setColor( Color.black );
	    g.drawRect( -wid, -21, wid * 2, anytext ? 57 : 42 );
	    g.drawRect( -wid - 1, -21 - 1, wid * 2 + 2, (anytext ? 57 : 42) + 2 );
	    */
	}
	
	drawIcon(g);

	//g.setColor( Color.lightGray );
	//g.drawString( name, -(stringWidth/2) + 4, textDown() + 4 );
	g.setColor( Color.black );
	g.drawString( name, -(stringWidth/2), textDown() );
	g.translate( -x, -y );
    }
}
