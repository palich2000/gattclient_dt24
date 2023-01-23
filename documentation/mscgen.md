# How to encode Message Sequence Charts within doxygen comments?

Message Sequence Chart doxygen addon [see](http://www.mcternan.me.uk/mscgen/) allows that

* Install the addon (according to the above documentation link)
* Encode comments according to thereafter syntax reference

# A first example

<pre>
<code>
<table border=0>
<tr>
<td>
	# MSC for some fictional process
	
	msc {
  		hscale = "2";

  		a,b,c;

  		a->b [ label = "ab()" ] ;
  		b->c [ label = "bc(TRUE)"];
  		c=>c [ label = "process(1)" ];
  		c=>c [ label = "process(2)" ];
  		...;
  		c=>c [ label = "process(n)" ];
  		c=>c [ label = "process(END)" ];
  		a<<=c [ label = "callback()"];
  		---  [ label = "If more to run", ID="*" ];
  		a->a [ label = "next()"];
  		a->c [ label = "ac1()\nac2()"];
  		b<-c [ label = "cb(TRUE)"];
  		b->b [ label = "stalled(...)"];
  		a<-b [ label = "ab() = FALSE"];
	}
</code>
</td>
<td><a href="img/example0.msc"><img src="img/example0.png" border="0" alt="Example rendered MSC"></a></td>
</tr>
</table>
</pre>

# Syntax reference

<table border="1">
 <tbody><tr>
    <th>Graphic</th>
    <th>Source Representation</th>
    <th>Meaning</th>
  </tr>
  <tr>
    <td><center><img src="img/msc-sig.png" alt="Rendered message arc"></center></td>
    <td><center>-&gt; or &lt;-</center></td>
    <td><center>Message</center></td>
  </tr>
  <tr>
    <td><center><img src="img/msc-func.png" alt="Rendered function call arc"></center></td>
    <td><center>=&gt; or &lt;=</center></td>
    <td><center>Method or function call</center></td>
  </tr>
  <tr>
    <td><center><img src="img/msc-retval.png" alt="Rendered return value arc"></center></td>
    <td><center>&gt;&gt; or &lt;&lt;</center></td>
    <td><center>Method or function return value</center></td>
  </tr>
  <tr>
    <td><center><img src="img/msc-callback.png" alt="Rendered callback arc"></center></td>
    <td><center>=&gt;&gt; or &lt;&lt;=</center></td>
    <td><center>Callback</center></td>
  </tr>
  <tr>
    <td><center><img src="img/msc-emph.png" alt="Rendered double message arc"></center></td>
    <td><center>:&gt; or &lt;:</center></td>
    <td><center>Emphasised Message</center></td>
  </tr>
  <tr>
    <td><center><img src="img/msc-lost.png" alt="Rendered lost message arc"></center></td>
    <td><center>-x or x-</center></td>
    <td><center>Lost Message</center></td>
  </tr>
  <tr>
    <td><center><img src="img/msc-time.png" alt="Rendered discontinuinity"></center></td>
    <td><center>...</center></td>
    <td><center>Indicates that some signals may be deliberately omitted
                from the msc, or that some large period of time may have passed.</center></td>
  </tr>
  <tr>
    <td><center><img src="img/msc-state.png" alt="Rendered block divider"></center></td>
    <td><center>---</center></td>
    <td><center>Used to add comment to a block of signals or indicate some
                action or state has occurred.</center></td>
  </tr>
  <tr>
    <td><center><img src="img/msc-spacer.png" alt="Rendered spacer"></center></td>
    <td><center>|||</center></td>
    <td><center>Used to add extra space between rows.</center></td>
  </tr>
  <tr>
    <td><center><img src="img/msc-broadcast.png" alt="Rendered broadcast arc"></center></td>
    <td><center>-&gt;* or *&lt;-</center></td>
    <td><center>Broadcast arcs, where the arc is extended to all but the source
                entity.  Any arc label is centred across the whole chart.</center></td>
  </tr>
  <tr>
    <td><center><img src="img/msc-box.png" alt="Rendered state box"></center></td>
    <td><center>box</center></td>
    <td><center>Box arcs, where the arc is replaced with a box between the selected
                entities.  Any arc label is centred in the box and word
                wrapped if needed.</center></td>
  </tr>
  <tr>
    <td><center><img src="img/msc-rbox.png" alt="Rendered rounded box"></center></td>
    <td><center>rbox</center></td>
    <td><center>Rounded box arcs, where the arc is replaced with a box between the
                selected entities.  Any arc label is centred in the box and word
                wrapped if needed.</center></td>
  </tr>
  <tr>
    <td><center><img src="img/msc-abox.png" alt="Rendered angular box"></center></td>
    <td><center>abox</center></td>
    <td><center>Angular box arcs, where the arc is replaced with a box between the
                selected entities.  Any arc label is centred in the box and word
                wrapped if needed.</center></td>
  </tr>
  <tr>
    <td><center><img src="img/msc-note.png" alt="Rendered note box"></center></td>
    <td><center>note</center></td>
    <td><center>Note box arcs, where the arc is replaced with a box between the
                selected entities.  Any arc label is centred in the box and word
                wrapped if needed.</center></td>
  </tr>
</tbody></table>

# How to use it within C code?

<pre>
<code>
/** @brief Fandango on core.
 * The Iberian dance Module is the place to implement any destructive functionality.
 * Currently the only supported dance is the "Fandango on core". but future enhancements
 * may allow this task to support a larger number of stack smashing. bit-rubble producing hacks.
 *
 *\msc
 *  T,"Iberian Dance Module";
 *
 *  T->"Iberian Dance Module" [label="IbFandango", URL="\ref IbFandango"];
 *  T<<"Iberian Dance Module" [label="void *", ID="1"];
 *\endmsc
 *
 * <OL>
 * <LI>In some cases, the system may have failed before the pointer can be returned.
 * </OL>
 * @returns The function is will attempt to return the corrupt pointer value although this cannot be guaranted
 */
void *IbFandango(){
..................
}
</code>
</pre>

Rendering follows<br/>
<img src="img/rendering.png" alt="Rendered Example">

