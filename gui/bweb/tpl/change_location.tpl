<br/>
<div class='titlediv'>
  <h1 class='newstitle'> 
   __Move media__
  </h1>
 </div>
 <div class='bodydiv'>

<form action="?" method='GET'>
<table>
<tr>
<td><b>__To:__ </b></td><td><input class='formulaire' name='email' value='<TMPL_VAR email>'></td>
</tr><tr>
<td><b>__Subject:__ </b></td><td><input class='formulaire' name='subject' value='__[BACULA] Move media to__ <TMPL_VAR newlocation>' size='80'></td>
</tr><tr>
<td></td>
<td>
<textarea name='content' class='formulaire' cols='80' rows='32'>
Hi,

Could you move these media to <TMPL_VAR newlocation>
Media :
<TMPL_LOOP media>
 - <TMPL_VAR VolumeName>  (<TMPL_VAR location>)
</TMPL_LOOP>

When it's finish, could you update media location ? 
(you can use this link : <TMPL_VAR url>).

Thanks
</textarea>
</td></tr></table>
<input class='formulaire' type='submit' name='action' value='move_email'>
</form>
<br>
<a href="<TMPL_VAR url>"><img alt='__Update now__' src='/bweb/update.png'>__Update now__</a>
</div>
