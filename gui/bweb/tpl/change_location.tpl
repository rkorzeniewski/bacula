<br/>
 <table class='box'>
  <tr><td> Move media </td></tr>
  <tr><td>
      <b>To: </b> <TMPL_VAR NAME=email> <br/>
      <b>Subject: </b> [BACULA] Move media to <TMPL_VAR NAME=newlocation> <br/>
<br/>
Dear,<br/>
<br/>
Could you move these media to <TMPL_VAR NAME=newlocation> <br/>
Media : <br/>
<ul>
<TMPL_LOOP NAME=Medias>
 <li> <TMPL_VAR NAME=VolumeName>  (<TMPL_VAR NAME=location>)
</TMPL_LOOP>
</ul>

When it's finish, could you update media location ? (you can use <a
href="<TMPL_VAR NAME=url>">this link</a>).
  <br/> 
Thanks

  </td></tr>
 </table>

