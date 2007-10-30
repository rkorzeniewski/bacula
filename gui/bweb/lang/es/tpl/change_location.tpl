<br/>
<div class='titlediv'>
  <h1 class='newstitle'> 
   Mover medio
  </h1>
 </div>
 <div class='bodydiv'>

<form action="?" method='GET'>
<table>
<tr>
<td><b>Para: </b></td><td><input class='formulaire' name='email' value='<TMPL_VAR email>'></td>
</tr><tr>
<td><b>Asunto: </b></td><td><input class='formulaire' name='subject' value='[BACULA] Mover medio a <TMPL_VAR newlocation>' size='80'></td>
</tr><tr>
<td></td>
<td>
<textarea name='content' class='formulaire' cols='80' rows='32'>
Estimado,

Puede mover este medio a <TMPL_VAR newlocation>
Medio :
<TMPL_LOOP media>
 - <TMPL_VAR VolumeName>  (<TMPL_VAR location>)
</TMPL_LOOP>

Cuando finalice, puede actualizar la ubicacion del medio ? 
(puede usar este link : <TMPL_VAR url>).

Gracias
</textarea>
</td></tr></table>
<input class='formulaire' type='submit' name='action' value='move_email'>
</form>
<br>
<a href="<TMPL_VAR url>"><img alt='update now' src='/bweb/update.png'>Actualizar Ahora</a>
</div>
